"""Compile supplied GLB resources to a bounded, versioned OpenGL runtime pack.

Reads Resource only. Does not alter Blender sources or their GLB delivery files.
Requires numpy and Pillow; no game build or execution.
"""
from pathlib import Path
import argparse, bisect, hashlib, io, json, math, struct
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "BaseProject_SE/Resource/Frontier_LowPoly_Kit_v1"
OUTPUT = ROOT / "BaseProject_SE/SimpleGame/Assets/World"
FPS = 20
CELL = 260
GRID = 8
DTYPE = np.dtype([("p", "<f4", 3), ("n", "<f4", 3), ("c", "<f4", 3),
                  ("uv", "<f4", 3), ("j", "<u2", 4), ("w", "<f4", 4)])
TEXTURES = [Image.new("RGBA", (256, 256), (255, 255, 255, 255))]
TEXTURE_IDS = {"white": 0}

def quaternion_matrix(q):
    x, y, z, w = q / max(np.linalg.norm(q), 1e-12)
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), 0],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), 0],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), 0],
                     [0, 0, 0, 1]], dtype=np.float64)

def slerp(a, b, t):
    dot = np.dot(a, b)
    if dot < 0:
        b, dot = -b, -dot
    if dot > .9995:
        v = a + t*(b-a)
        return v / max(np.linalg.norm(v), 1e-12)
    theta = math.acos(max(-1, min(1, dot)))
    return (math.sin((1-t)*theta)*a + math.sin(t*theta)*b)/math.sin(theta)

class Glb:
    def __init__(self, path):
        self.path = path
        data = path.read_bytes()
        magic, version, length = struct.unpack_from("<III", data)
        assert magic == 0x46546c67 and version == 2 and length == len(data)
        size, kind = struct.unpack_from("<II", data, 12)
        assert kind == 0x4e4f534a
        self.g = json.loads(data[20:20+size])
        offset = 20+size
        size, kind = struct.unpack_from("<II", data, offset)
        assert kind == 0x004e4942
        self.binary = data[offset+8:offset+8+size]
        self.cache = {}
        self.parents = {}
        for i, n in enumerate(self.g["nodes"]):
            for child in n.get("children", []):
                self.parents[child] = i
        self.bind = self.globals({})

    def accessor(self, index):
        if index in self.cache:
            return self.cache[index]
        a = self.g["accessors"][index]
        assert "sparse" not in a
        view = self.g["bufferViews"][a["bufferView"]]
        fmt = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}[a["componentType"]]
        count = {"SCALAR":1,"VEC2":2,"VEC3":3,"VEC4":4,"MAT4":16}[a["type"]]
        item = np.dtype(fmt).itemsize
        offset = view.get("byteOffset", 0)+a.get("byteOffset", 0)
        stride = view.get("byteStride", item*count)
        result = np.ndarray((a["count"], count), dtype=fmt, buffer=self.binary,
                            offset=offset, strides=(stride,item)).copy()
        if a.get("normalized") and a["componentType"] != 5126:
            info = np.iinfo(result.dtype)
            result = np.maximum(result.astype(float)/info.max, -1)
        self.cache[index] = result
        return result

    def globals(self, overrides):
        local = []
        for i, n in enumerate(self.g["nodes"]):
            if "matrix" in n:
                assert i not in overrides
                local.append(np.array(n["matrix"]).reshape(4,4).T)
                continue
            o = overrides.get(i, {})
            t = np.array(o.get("translation", n.get("translation", [0,0,0])))
            r = np.array(o.get("rotation", n.get("rotation", [0,0,0,1])))
            scale = np.array(o.get("scale", n.get("scale", [1,1,1])))
            m = quaternion_matrix(r)
            m[:3,:3] *= scale[None,:]
            m[:3,3] = t
            local.append(m)
        result = {}
        def resolve(i):
            if i not in result:
                result[i] = resolve(self.parents[i]) @ local[i] if i in self.parents else local[i]
            return result[i]
        for i in range(len(local)):
            resolve(i)
        return result

    def image_id(self, texture):
        image = self.g["images"][self.g["textures"][texture]["source"]]
        view = self.g["bufferViews"][image["bufferView"]]
        offset = view.get("byteOffset",0)
        data = self.binary[offset:offset+view["byteLength"]]
        key = hashlib.sha256(data).hexdigest()
        if key not in TEXTURE_IDS:
            TEXTURE_IDS[key] = len(TEXTURES)
            TEXTURES.append(Image.open(io.BytesIO(data)).convert("RGBA").resize((256,256), Image.Resampling.LANCZOS))
        return TEXTURE_IDS[key]

    def compile(self, name):
        vertices, indices = [], []
        skin_nodes = [(i,n) for i,n in enumerate(self.g["nodes"]) if "mesh" in n and "skin" in n]
        joints, inverse_bind = [], None
        mesh_inverse = np.eye(4)
        if skin_nodes:
            skin = self.g["skins"][skin_nodes[0][1]["skin"]]
            joints = skin["joints"]
            assert len(joints) <= 64
            inverse_bind = self.accessor(skin["inverseBindMatrices"]).reshape(-1,4,4).transpose(0,2,1)
            mesh_inverse = np.linalg.inv(self.bind[skin_nodes[0][0]])
            for i, n in skin_nodes:
                assert n["skin"] == skin_nodes[0][1]["skin"]
                assert np.allclose(self.bind[i], np.linalg.inv(mesh_inverse), atol=1e-5)
        offset = 0
        for i,n in enumerate(self.g["nodes"]):
            if "mesh" not in n:
                continue
            m = self.bind[i]
            for primitive in self.g["meshes"][n["mesh"]]["primitives"]:
                assert primitive.get("mode",4) == 4
                attrs = primitive["attributes"]
                pos = self.accessor(attrs["POSITION"])
                normal = self.accessor(attrs["NORMAL"])
                v = np.zeros(len(pos), dtype=DTYPE)
                v["p"] = (m @ np.column_stack((pos,np.ones(len(pos)))).T).T[:,:3]
                v["n"] = (np.linalg.inv(m[:3,:3]).T @ normal.T).T
                v["n"] /= np.maximum(np.linalg.norm(v["n"],axis=1)[:,None],1e-8)
                material = self.g.get("materials",[{}])[primitive.get("material",0)]
                pbr = material.get("pbrMetallicRoughness",{})
                color = np.array(pbr.get("baseColorFactor",[1,1,1,1])[:3])
                # The existing renderer uses display-space colors.
                v["c"] = np.where(color<=.0031308,color*12.92,1.055*color**(1/2.4)-.055)
                tex = self.image_id(pbr["baseColorTexture"]["index"]) if "baseColorTexture" in pbr else 0
                if "TEXCOORD_0" in attrs:
                    v["uv"][:,:2] = self.accessor(attrs["TEXCOORD_0"])
                v["uv"][:,2] = tex
                if "JOINTS_0" in attrs:
                    v["j"] = self.accessor(attrs["JOINTS_0"])
                    v["w"] = self.accessor(attrs["WEIGHTS_0"])
                    v["w"] /= np.maximum(v["w"].sum(axis=1)[:,None],1e-8)
                    assert v["j"].max() < len(joints)
                ind = self.accessor(primitive["indices"]).reshape(-1).astype("<u4")
                if np.linalg.det(m[:3,:3]) < 0:
                    ind = ind.reshape(-1,3)[:,[0,2,1]].reshape(-1)
                indices.append(ind+offset)
                vertices.append(v)
                offset += len(v)
        v = np.concatenate(vertices)
        ind = np.concatenate(indices).astype("<u4")
        clips = []
        if joints:
            for animation in self.g.get("animations",[]):
                channels = []
                duration = 0
                for channel in animation["channels"]:
                    sampler = animation["samplers"][channel["sampler"]]
                    assert sampler.get("interpolation","LINEAR") in ("LINEAR","STEP")
                    times = self.accessor(sampler["input"]).reshape(-1)
                    values = self.accessor(sampler["output"])
                    duration = max(duration,float(times[-1]))
                    channels.append((channel["target"]["node"],channel["target"]["path"],
                                     times,values,sampler.get("interpolation","LINEAR")))
                frames = max(2,math.ceil(duration*FPS)+1)
                matrices = []
                for frame in range(frames):
                    time = duration*frame/(frames-1)
                    overrides = {}
                    for node, prop, times, values, interpolation in channels:
                        index = min(max(0,bisect.bisect_right(times,time)-1),len(times)-1)
                        j = min(index+1,len(times)-1)
                        t = (time-times[index])/(times[j]-times[index]) if j!=index else 0
                        if interpolation == "STEP":
                            value = values[index]
                        elif prop == "rotation":
                            value = slerp(values[index],values[j],t)
                        else:
                            value = values[index]*(1-t)+values[j]*t
                        overrides.setdefault(node,{})[prop] = value
                    global_pose = self.globals(overrides)
                    # Matrices transform the exported world-space bind vertices.
                    matrices.append([global_pose[j] @ inverse_bind[b] @ mesh_inverse for b,j in enumerate(joints)])
                matrices = np.array(matrices,dtype="<f4").transpose(0,1,3,2).copy()
                assert np.isfinite(matrices).all()
                clips.append((animation["name"],duration,frames,matrices))
        bounds = np.concatenate([v["p"].min(axis=0),v["p"].max(axis=0)]).astype("<f4")
        return {"name":name,"vertices":v,"indices":ind,"bones":len(joints),"clips":clips,"bounds":bounds,
                "source":str(self.path.relative_to(SOURCE))}

def string(out, value):
    raw = value.encode("utf-8")
    out.write(struct.pack("<I",len(raw)))
    out.write(raw)

def compile_pack():
    manifest = json.loads((SOURCE/"Documentation/asset_manifest.json").read_text(encoding="utf-8-sig"))
    selected = [a for a in manifest["assets"] if a["kind"] in ("human_preset","quadruped","settlement_module","settlement_prefab","nature_module","terrain_tile")]
    # Use the delivered civilian body for the player; the commander mesh stays available as a reference.
    assets = [Glb(SOURCE/a["file"]).compile(a["name"]) for a in selected]
    assert len(TEXTURES) <= GRID*GRID
    atlas = Image.new("RGBA",(GRID*CELL,GRID*CELL),(255,255,255,255))
    for i,image in enumerate(TEXTURES):
        x,y=(i%GRID)*CELL,(i//GRID)*CELL
        atlas.paste(image,(x+2,y+2))
        atlas.paste(image.crop((0,0,1,256)).resize((2,256)),(x,y+2))
        atlas.paste(image.crop((255,0,256,256)).resize((2,256)),(x+258,y+2))
        atlas.paste(image.crop((0,0,256,1)).resize((260,2)),(x,y))
        atlas.paste(image.crop((0,255,256,256)).resize((260,2)),(x,y+258))
    OUTPUT.mkdir(parents=True,exist_ok=True)
    destination = OUTPUT/"kingdom.rpack"
    with destination.open("wb") as out:
        out.write(b"RPK1")
        out.write(struct.pack("<IIII",1,len(assets),atlas.width,atlas.height))
        out.write(atlas.tobytes())
        for a in assets:
            string(out,a["name"])
            out.write(struct.pack("<IIII",len(a["vertices"]),len(a["indices"]),a["bones"],len(a["clips"])))
            out.write(a["bounds"].tobytes())
            out.write(a["vertices"].tobytes())
            out.write(a["indices"].tobytes())
            for name,duration,frames,matrices in a["clips"]:
                string(out,name)
                out.write(struct.pack("<fI",duration,frames))
                out.write(matrices.tobytes())
    report={"format":"RPK1","version":1,"animation_fps":FPS,"texture_cells":len(TEXTURES),
            "source":"Resource/Frontier_LowPoly_Kit_v1","bytes":destination.stat().st_size,
            "sha256":hashlib.sha256(destination.read_bytes()).hexdigest(),
            "assets":[{"name":a["name"],"source":a["source"],"vertices":len(a["vertices"]),
                       "triangles":len(a["indices"])//3,"bones":a["bones"],
                       "bounds":a["bounds"].tolist(),"clips":[c[0] for c in a["clips"]]} for a in assets]}
    (OUTPUT/"manifest.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k!="assets"},ensure_ascii=False))
    print("Compiled",len(assets),"assets without modifying Resource.")

if __name__ == "__main__":
    compile_pack()
