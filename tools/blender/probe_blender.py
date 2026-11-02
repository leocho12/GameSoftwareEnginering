"""Check the project's Blender CLI connection without opening or saving user scenes."""

import argparse
import json
import os
from pathlib import Path
import sys

import bpy


def available(operator):
    try:
        operator.get_rna_type()
        return True
    except (AttributeError, RuntimeError):
        return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    mesh = bpy.data.meshes.new("CodexConnectionProbe")
    try:
        mesh.from_pydata([(0, 0, 0), (1, 0, 0), (0, 1, 0)], [], [(0, 1, 2)])
        mesh.update()
        mesh_ready = len(mesh.vertices) == 3 and len(mesh.polygons) == 1
    finally:
        bpy.data.meshes.remove(mesh)

    report = {
        "schema_version": 1,
        "connection_type": "blender_background_python",
        "ready": mesh_ready,
        "executable_path": bpy.app.binary_path,
        "blender_version": bpy.app.version_string,
        "background": bpy.app.background,
        "process_id": os.getpid(),
        "project_root": str(Path(__file__).resolve().parents[2]),
        "mesh_creation_verified": mesh_ready,
        "export_gltf_available": available(bpy.ops.export_scene.gltf),
        "export_obj_available": available(bpy.ops.wm.obj_export),
        "saved_scene": False,
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=True), flush=True)
    if not mesh_ready:
        raise RuntimeError("Blender mesh creation probe failed.")


if __name__ == "__main__":
    main()
