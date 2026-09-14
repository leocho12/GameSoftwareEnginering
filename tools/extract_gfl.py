"""Read selected client bundles; export exact TextAssets and decoded textures.
Usage: python tools/extract_gfl.py CLIENT_ANDROID_NEW OUTPUT [characters...]
Install UnityPy==1.25.3 in .asset-tools (build-time tool only).
"""
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.asset-tools'))
import UnityPy

def main():
    source, output = map(pathlib.Path, sys.argv[1:3])
    names = sys.argv[3:] or ['4type', 'm4a1', 'ump45', 'hk416', 'g11']
    for name in names:
        candidates = list(source.glob('*character' + name + 'spine.ab'))
        if len(candidates) != 1:
            raise ValueError(f'{name}: expected one bundle, found {len(candidates)}')
        bundle = candidates[0]
        directory = output / name
        directory.mkdir(parents=True, exist_ok=True)
        records = []
        env = UnityPy.load(str(bundle))
        for obj in env.objects:
            if obj.type.name not in ('TextAsset', 'Texture2D'):
                continue
            data = obj.read()
            filename = data.m_Name
            if pathlib.Path(filename).name != filename or '/' in filename or '\\' in filename:
                raise ValueError('Unexpected asset path')
            if obj.type.name == 'Texture2D':
                filename += '.png'
                data.image.save(directory / filename)
            else:
                content = data.m_Script
                if isinstance(content, str):
                    content = content.encode('utf-8', 'surrogateescape')
                (directory / filename).write_bytes(content)
            target = directory / filename
            records.append(dict(file=filename, type=obj.type.name, bytes=target.stat().st_size,
                                sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
        (directory / 'source.json').write_text(json.dumps(dict(bundle=bundle.name,
            sha256=hashlib.sha256(bundle.read_bytes()).hexdigest(), assets=records), indent=2), encoding='utf-8')
        print(name, [(r['file'], r['bytes']) for r in records])

if __name__ == '__main__':
    main()
