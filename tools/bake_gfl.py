"""Bake converted GFL 2.1 skeletons into ordered, textured triangle frames.

This is an offline data processor, not a Spine runtime. Input features are
checked explicitly; unsupported constraints fail instead of silently dropping.
GFA1: little endian, clip count; each clip has name[32], duration, frame count;
each frame has vertex count followed by ordered float32 x,y,u,v,r,g,b,a vertices.
"""
import argparse
import bisect
import json
import math
import pathlib
import shutil
import struct


def atlas_read(path):
    regions = {}
    current = None
    page = None
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        if ':' not in line:
            if page is None:
                page = line.strip()
            else:
                current = regions.setdefault(line.strip(), {})
        elif line.startswith(' ') and current is not None:
            k, v = line.strip().split(':', 1)
            current[k] = v.strip()
    for r in regions.values():
        for k in ('xy', 'size', 'orig', 'offset'):
            r[k] = tuple(map(int, r[k].split(',')))
        r['rotate'] = r['rotate'] == 'true'
    return page, regions


def curve_factor(curve, x):
    if curve == 'stepped':
        return 0.0
    if not isinstance(curve, list):
        return x
    x1, y1, x2, y2 = curve
    lo, hi = 0.0, 1.0
    def bez(a, b, t):
        return 3*(1-t)**2*t*a + 3*(1-t)*t*t*b + t**3
    for _ in range(15):
        mid = (lo+hi)/2
        if bez(x1, x2, mid) < x:
            lo = mid
        else:
            hi = mid
    return bez(y1, y2, (lo+hi)/2)


def keys_at(frames, t):
    if not frames or t < frames[0].get('time', 0):
        return None, None, 0
    i = bisect.bisect_right([f.get('time', 0) for f in frames], t)-1
    a = frames[i]
    if i+1 == len(frames):
        return a, a, 0
    b = frames[i+1]
    f = (t-a.get('time', 0))/(b['time']-a.get('time', 0))
    return a, b, curve_factor(a.get('curve'), f)


def value(frames, t, key, default, angle=False):
    a, b, f = keys_at(frames, t)
    if a is None:
        return default
    av, bv = a.get(key, default), b.get(key, default)
    delta = bv-av
    if angle:
        delta = (delta+180)%360-180
    return av+delta*f


def rgba(s):
    return [int(s[i:i+2], 16)/255 for i in range(0, 8, 2)]


def duration(data):
    if isinstance(data, dict):
        return max([float(data.get('time', 0))] + [duration(v) for v in data.values()])
    if isinstance(data, list):
        return max([0]+[duration(v) for v in data])
    return 0


class Baker:
    def __init__(self, json_path, atlas_path):
        from PIL import Image
        self.data = json.loads(json_path.read_text())
        self.page, self.regions = atlas_read(atlas_path)
        self.image_path = atlas_path.parent/self.page
        self.image_size = Image.open(self.image_path).size
        assert self.data['skeleton']['spine'] == '2.1.27'
        assert not self.data.get('ik'), 'IK constraints require explicit support'
        for a in self.data['animations'].values():
            assert not a.get('ik'), 'IK animation unsupported'
        assert all(s.get('blend', 'normal') == 'normal' for s in self.data['slots'])
        assert all(b.get('inheritScale', True) and b.get('inheritRotation', True) for b in self.data['bones'])
        self.bone_indices = {b['name']: i for i, b in enumerate(self.data['bones'])}
        self.slot_indices = {s['name']: i for i, s in enumerate(self.data['slots'])}
        self.origin_y = 0
        self.scale = 1
        setup = self.pose('wait', 0)
        self.origin_y = min(v[1] for v in setup)
        self.scale = 1.8/(max(v[1] for v in setup)-self.origin_y)

    def transform(self, p, x, y):
        a, b, c, d, tx, ty = p[:6]
        return a*x+b*y+tx, c*x+d*y+ty

    def pose(self, name, t):
        anim = self.data['animations'][name]
        world = []
        for bone in self.data['bones']:
            tracks = anim.get('bones', {}).get(bone['name'], {})
            x = bone.get('x', 0)+value(tracks.get('translate'), t, 'x', 0)
            y = bone.get('y', 0)+value(tracks.get('translate'), t, 'y', 0)
            rot = bone.get('rotation', 0)+value(tracks.get('rotate'), t, 'angle', 0, True)
            sx = bone.get('scaleX', 1)*value(tracks.get('scale'), t, 'x', 1)
            sy = bone.get('scaleY', 1)*value(tracks.get('scale'), t, 'y', 1)
            fx, fy = bone.get('flipX', False), bone.get('flipY', False)
            a, _, _ = keys_at(tracks.get('flipX'), t)
            if a is not None:
                fx = a['x']
            a, _, _ = keys_at(tracks.get('flipY'), t)
            if a is not None:
                fy = a['y']
            if bone.get('parent'):
                parent = world[self.bone_indices[bone['parent']]]
                x, y = self.transform(parent, x, y)
                rot += parent[6]
                sx *= parent[7]
                sy *= parent[8]
                fx, fy = fx != parent[9], fy != parent[10]
            r = math.radians(rot)
            co, si = math.cos(r), math.sin(r)
            mx, my = (-1 if fx else 1), (-1 if fy else 1)
            world.append((co*sx*mx, -si*sy*mx, si*sx*my, co*sy*my, x, y, rot, sx, sy, fx, fy))

        slots = self.data['slots']
        order = list(range(len(slots)))
        draw, _, _ = keys_at(anim.get('drawOrder'), t)
        if draw and draw.get('offsets'):
            order = [-1]*len(slots)
            rest = []
            source = 0
            for off in draw['offsets']:
                index = self.slot_indices[off['slot']]
                while source < index:
                    rest.append(source)
                    source += 1
                order[source+off['offset']] = source
                source += 1
            rest.extend(range(source, len(slots)))
            it = iter(rest)
            order = [next(it) if i == -1 else i for i in order]
        result = []
        skin = self.data['skins']['default']
        for index in order:
            slot = slots[index]
            if slot['name'].lower() == 'shadow':
                continue  # The 3D renderer supplies a ground shadow.
            tracks = anim.get('slots', {}).get(slot['name'], {})
            chosen = slot.get('attachment')
            a, _, _ = keys_at(tracks.get('attachment'), t)
            if a is not None:
                chosen = a.get('name')
            if not chosen:
                continue
            attachment = skin[slot['name']][chosen]
            if attachment['type'] == 'boundingbox':
                continue
            r = self.regions[attachment.get('path', chosen)]
            color = rgba(slot.get('color', 'ffffffff'))
            a, b, f = keys_at(tracks.get('color'), t)
            if a is not None:
                color = [x+(y-x)*f for x, y in zip(rgba(a['color']), rgba(b['color']))]
            color = [x*y for x, y in zip(color, rgba(attachment.get('color', 'ffffffff')))]
            if color[3] <= 0:
                continue
            transform = world[self.bone_indices[slot['bone']]]
            mesh_type = attachment['type']
            if mesh_type == 'region':
                ow, oh = r['orig']
                rw, rh = r['size']
                ox, oy = r['offset']
                w, h = attachment['width'], attachment['height']
                left, bottom = (ox/ow-.5)*w, (oy/oh-.5)*h
                right, top = left+rw/ow*w, bottom+rh/oh*h
                co = math.cos(math.radians(attachment.get('rotation', 0)))
                si = math.sin(math.radians(attachment.get('rotation', 0)))
                points = []
                for x, y in ((left, bottom), (left, top), (right, top), (right, bottom)):
                    x *= attachment.get('scaleX', 1)
                    y *= attachment.get('scaleY', 1)
                    points.append(self.transform(transform, co*x-si*y+attachment.get('x', 0), si*x+co*y+attachment.get('y', 0)))
                uvs, triangles = [0, 1, 0, 0, 1, 0, 1, 1], [0, 1, 2, 2, 3, 0]
            else:
                assert mesh_type in ('mesh', 'skinnedmesh'), mesh_type
                vertices = attachment['vertices']
                ffd = anim.get('ffd', {}).get('default', {}).get(slot['name'], {}).get(chosen)
                a, b, f = keys_at(ffd, t)
                deformation = None if a is None else [x+(y-x)*f for x, y in zip(a['vertices'], b['vertices'])]
                uvs, triangles = attachment['uvs'], attachment['triangles']
                if mesh_type == 'mesh':
                    vv = vertices if deformation is None else deformation
                    points = [self.transform(transform, *vv[i:i+2]) for i in range(0, len(vv), 2)]
                else:
                    points = []
                    p = di = 0
                    while p < len(vertices):
                        count = int(vertices[p]); p += 1
                        px = py = 0.0
                        for _ in range(count):
                            bone, x, y, weight = vertices[p:p+4]; p += 4
                            if deformation is not None:
                                x += deformation[di]; y += deformation[di+1]
                            di += 2
                            wx, wy = self.transform(world[int(bone)], x, y)
                            px += wx*weight; py += wy*weight
                        points.append((px, py))
            assert len(points)*2 == len(uvs)
            tw, th = self.image_size
            for i in triangles:
                x, y = points[i]
                u, v = uvs[i*2:i*2+2]
                rx, ry = r['xy']; rw, rh = r['size']
                if r['rotate']:
                    u, v = (rx+v*rh)/tw, (ry+(1-u)*rw)/th
                else:
                    u, v = (rx+u*rw)/tw, (ry+v*rh)/th
                vertex = (x*self.scale, (y-self.origin_y)*self.scale, u, v, *color)
                assert all(math.isfinite(n) for n in vertex)
                assert -.001 <= u <= 1.001 and -.001 <= v <= 1.001
                result.append(vertex)
        return result


def bake(source, output, fps):
    output.mkdir(parents=True, exist_ok=True)
    report = []
    for model in ('4type', 'M4A1', 'UMP45', 'HK416', 'G11'):
        directory = source/model.lower()
        baker = Baker(directory/(model+'.json'), directory/(model+'.atlas'))
        clips = ('wait', 'move', 'attack', 'die', 'victory', 'victoryloop')
        path = output/(model.lower()+'.gfa')
        record = dict(model=model, texture=model.lower()+'.png', clips={})
        with path.open('wb') as out:
            out.write(b'GFA1'+struct.pack('<I', len(clips)))
            for clip in clips:
                length = max(duration(baker.data['animations'][clip]), 1/fps)
                count = max(2, math.ceil(length*fps)+1)
                out.write(clip.encode().ljust(32, b'\0')+struct.pack('<fI', length, count))
                for frame in range(count):
                    vertices = baker.pose(clip, frame/(count-1)*length)
                    out.write(struct.pack('<I', len(vertices)))
                    for vertex in vertices:
                        out.write(struct.pack('<8f', *vertex))
                record['clips'][clip] = dict(duration=length, frames=count)
        shutil.copyfile(baker.image_path, output/(model.lower()+'.png'))
        record['bytes'] = path.stat().st_size
        report.append(record)
        print(model, record['bytes'], record['clips'])
    (output/'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--fps', type=int, default=30)
    args = parser.parse_args()
    assert 1 <= args.fps <= 60
    bake(args.source, args.output, args.fps)
