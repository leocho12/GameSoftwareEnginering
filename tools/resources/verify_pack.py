"""Independent reader and integrity checks for the exact C++ runtime resource layout."""
from pathlib import Path
import hashlib, json, struct
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
PACK=ROOT/"BaseProject_SE/SimpleGame/Assets/World/kingdom.rpack"
FORMAT=np.dtype([("p","<f4",3),("n","<f4",3),("c","<f4",3),("uv","<f4",3),("j","<u2",4),("w","<f4",4)])
assert FORMAT.itemsize==72
data=PACK.read_bytes()
offset=0
def read(fmt):
    global offset
    result=struct.unpack_from("<"+fmt,data,offset)
    offset+=struct.calcsize("<"+fmt)
    return result
def text():
    global offset
    size,=read("I")
    assert size<=256
    value=data[offset:offset+size].decode("utf-8");offset+=size
    return value
assert data[:4]==b"RPK1"
offset=4
version,count,width,height=read("4I")
assert version==1 and 1<=count<=256 and width==height==2080
offset+=width*height*4
assets={}
for _ in range(count):
    name=text()
    vertices,indices,bones,clips=read("4I")
    bounds=np.array(read("6f"))
    v=np.frombuffer(data,dtype=FORMAT,count=vertices,offset=offset);offset+=vertices*72
    ix=np.frombuffer(data,dtype="<u4",count=indices,offset=offset);offset+=indices*4
    assert indices%3==0 and ix.max()<vertices
    assert np.isfinite(v["p"]).all() and np.isfinite(v["n"]).all()
    assert np.isfinite(v["w"]).all() and (v["uv"][:,2]>=0).all() and (v["uv"][:,2]<64).all()
    assert np.allclose(v["p"].min(axis=0),bounds[:3]) and np.allclose(v["p"].max(axis=0),bounds[3:])
    if bones:
        assert (v["j"]<bones).all() and np.allclose(v["w"].sum(axis=1),1,atol=1e-5)
    names=[]
    for _ in range(clips):
        clip=text();duration,frames=read("fI")
        assert duration>=0 and frames>=2 and bones<=64
        size=frames*bones*16
        matrices=np.frombuffer(data,dtype="<f4",count=size,offset=offset).reshape(frames,bones,4,4).transpose(0,1,3,2)
        offset+=size*4
        assert np.isfinite(matrices).all()
        assert np.allclose(matrices[:,:,3,:],np.array([0,0,0,1]),atol=1e-5)
        # Exercise the C++ weighted skinning formula in neutral, motion and attack frames.
        if clip in ["Idle","Walk","Run","Attack"]:
            for frame in [0,frames//2,frames-1]:
                pos=np.column_stack((v["p"],np.ones(vertices)))
                sample=matrices[frame]
                skinned=np.zeros((vertices,4))
                for influence in range(4):
                    transformed=np.einsum("vij,vj->vi",sample[v["j"][:,influence]],pos)
                    skinned+=v["w"][:,influence,None]*transformed
                assert np.isfinite(skinned).all()
                assert np.allclose(skinned[:,3],1,atol=1e-4)
                assert np.max(np.abs(skinned[:,:3]))<12, (name,clip,"exploded skeleton")
        names.append(clip)
    if bones:
        assert all(c in names for c in ["Idle","Walk","Run","Attack","Pose_Neutral"])
    assert name not in assets
    assets[name]={"vertices":vertices,"triangles":indices//3,"bones":bones,"clips":len(names)}
assert offset==len(data)
required=["NPC_Worker","NPC_Resident","NPC_Scout","Wolf","Tiger","Fox","Deer","Home_Prefab",
          "Tree_Pine_01","Tree_Broadleaf_01","Rock_01","Rock_02","Terrain_Grass_2x2",
          "Terrain_Soil_2x2","Grass_Patch_01","Table_Work","Chair","Supply_Crate","Sack",
          "Bucket","Lamp_Utility","Fence_Timber_2m","Cooking_Pot"]
assert all(n in assets for n in required)
manifest=json.loads((PACK.parent/"manifest.json").read_text(encoding="utf-8"))
assert manifest["sha256"]==hashlib.sha256(data).hexdigest()
print(f"Resource pack passed: {len(assets)} meshes, {sum(a['clips'] for a in assets.values())} clips, atlas, indices, weights, bounds, sampled skinning, SHA-256.")
