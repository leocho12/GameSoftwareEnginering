"""Validate baked streams and make an asset-only contact sheet (no game launch)."""
import json
import pathlib
import struct
import numpy as np
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSETS = ROOT/'BaseProject_SE/SimpleGame/Assets/GFL'


def read(path):
    data = path.read_bytes()
    assert data[:4] == b'GFA1'
    count, = struct.unpack_from('<I', data, 4)
    offset = 8
    clips = {}
    for _ in range(count):
        name = data[offset:offset+32].split(b'\0')[0].decode(); offset += 32
        duration, frames = struct.unpack_from('<fI', data, offset); offset += 8
        assert duration > 0 and 2 <= frames <= 1000
        values = []
        for _ in range(frames):
            size, = struct.unpack_from('<I', data, offset); offset += 4
            assert size % 3 == 0 and size < 100000
            vertices = np.frombuffer(data, '<f4', size*8, offset).reshape(-1, 8); offset += size*32
            assert np.isfinite(vertices).all()
            assert (vertices[:, 2:8] >= 0).all() and (vertices[:, 2:8] <= 1).all()
            assert np.abs(vertices[:, :2]).max() < 20
            values.append(vertices)
        assert not np.array_equal(values[0], values[len(values)//2]), f'Static clip: {name}'
        clips[name] = (duration, values)
    assert offset == len(data), 'Trailing or missing data'
    return clips


def render(vertices, texture):
    image = np.zeros((256, 256, 4), dtype=np.float32)
    image[:, :, :3] = [.14, .18, .2]; image[:, :, 3] = 1
    h, w = texture.shape[:2]
    for tri in vertices.reshape(-1, 3, 8):
        positions = tri[:, :2]*[105, -105]+[128, 230]
        x0, y0 = np.maximum(np.floor(positions.min(axis=0)).astype(int), 0)
        x1, y1 = np.minimum(np.ceil(positions.max(axis=0)).astype(int)+1, 256)
        if x1 <= x0 or y1 <= y0:
            continue
        a, b, c = positions
        det = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(det) < 1e-7:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1]; xx = xx+.5; yy = yy+.5
        wa = ((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/det
        wb = ((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/det
        wc = 1-wa-wb
        mask = (wa >= 0)&(wb >= 0)&(wc >= 0)
        uv = wa[..., None]*tri[0, 2:4]+wb[..., None]*tri[1, 2:4]+wc[..., None]*tri[2, 2:4]
        tx = np.clip((uv[..., 0]*w).astype(int), 0, w-1)
        ty = np.clip((uv[..., 1]*h).astype(int), 0, h-1)
        src = texture[ty, tx]*tri[0, 4:8]
        alpha = src[..., 3:4]*mask[..., None]
        dst = image[y0:y1, x0:x1]
        dst[..., :3] = src[..., :3]*alpha+dst[..., :3]*(1-alpha)
    return Image.fromarray((np.clip(image, 0, 1)*255).astype('uint8'))


def main():
    sheet = Image.new('RGBA', (256*5, 280*3), '#232e33')
    for col, name in enumerate(('4type', 'm4a1', 'ump45', 'hk416', 'g11')):
        clips = read(ASSETS/(name+'.gfa'))
        texture = np.array(Image.open(ASSETS/(name+'.png')).convert('RGBA'), dtype=np.float32)/255
        for row, clip in enumerate(('wait', 'move', 'attack')):
            values = clips[clip][1]
            sheet.paste(render(values[len(values)//3], texture), (col*256, row*280))
            ImageDraw.Draw(sheet).text((col*256+12, row*280+256), name+' / '+clip, fill='white')
        print(name, {n: len(v[1]) for n, v in clips.items()}, 'validated')
    out = ROOT/'docs/asset-previews'
    out.mkdir(exist_ok=True)
    sheet.save(out/'gfl-contact-sheet.png')


if __name__ == '__main__':
    main()
