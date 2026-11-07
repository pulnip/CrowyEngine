"""FBX export with the project's axis and scale settings, and the prop file.

Axes are exported unconverted (Z up); the engine's loader converts them.

Exports are byte-for-byte reproducible:
- the header time and the Blender and FBX add-on versions in the header are
  pinned, so a Blender point release does not rewrite every model;
- object UUIDs come from SHA-256, not Python's per-process randomized hash();
- the exporter writes objects sorted by name and materials in the order it
  first meets them, which kit.build makes the palette's.
An input hash in a dot-prefixed sidecar skips the export when nothing that
feeds it changed.
"""
import contextlib
import datetime
import hashlib
import json
import types

import bpy
import io_scene_fbx
from io_scene_fbx import export_fbx_bin, fbx_utils

from . import ROOT

PINNED_TIME = datetime.datetime(2026, 1, 1)
PINNED_VERSION = '5.2.0 LTS'
PINNED_ADDON_VERSION = (5, 15, 0)
LIBRARY = ROOT / 'Tools' / 'Blender' / 'propkit'
GENERATOR = ROOT / 'Tools' / 'Blender' / 'generate.py'
MOVING_KEYS = ('name', 'kind', 'anchor', 'axis', 'range')


@contextlib.contextmanager
def deterministic_fbx():
    header = export_fbx_bin.fbx_header_elements
    key_to_uuid = fbx_utils._key_to_uuid

    def pinned_header(root, scene_data, time=None):
        # The header reads only bpy.app.version_string, bpy.data and the
        # add-on's bl_info.
        addon_version = io_scene_fbx.bl_info['version']
        export_fbx_bin.bpy = types.SimpleNamespace(
            app=types.SimpleNamespace(version_string=PINNED_VERSION),
            data=bpy.data)
        io_scene_fbx.bl_info['version'] = PINNED_ADDON_VERSION
        try:
            return header(root, scene_data, PINNED_TIME)
        finally:
            export_fbx_bin.bpy = bpy
            io_scene_fbx.bl_info['version'] = addon_version

    def stable_uuid(uuids, key):
        if not (isinstance(key, int) and 0 <= key < 2**63):
            digest = hashlib.sha256(repr(key).encode()).digest()
            key = int.from_bytes(digest[:8], 'little') >> 1
        return key_to_uuid(uuids, key)

    export_fbx_bin.fbx_header_elements = pinned_header
    fbx_utils._key_to_uuid = stable_uuid
    try:
        yield
    finally:
        export_fbx_bin.fbx_header_elements = header
        fbx_utils._key_to_uuid = key_to_uuid


def input_hash(asset_path):
    """SHA-256 over the library, the generator, the asset and Blender."""
    digest = hashlib.sha256()
    sources = sorted(LIBRARY.glob('*.py')) + [GENERATOR, asset_path]
    for path in sources:
        digest.update(path.relative_to(ROOT).as_posix().encode())
        digest.update(path.read_bytes().replace(b'\r\n', b'\n'))
    digest.update(bpy.app.version_string.encode())
    digest.update(bpy.app.build_hash)
    return digest.hexdigest()


def sidecar_of(path):
    return path.with_name(f'.{path.stem}.inputhash')


def prop_file_of(path):
    return path.with_suffix('.prop.json')


def is_up_to_date(path, digest):
    sidecar = sidecar_of(path)
    return (path.exists() and prop_file_of(path).exists() and sidecar.exists()
            and sidecar.read_text(encoding='utf-8').strip() == digest)


def export_fbx(path, objects, digest):
    """Export only the given objects (the prop, never preview helpers)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    with deterministic_fbx():
        bpy.ops.export_scene.fbx(
            filepath=str(path),
            use_selection=True,
            object_types={'MESH', 'OTHER', 'EMPTY'},
            use_mesh_modifiers=True,
            mesh_smooth_type='FACE',
            apply_unit_scale=True,
            apply_scale_options='FBX_SCALE_ALL',
            axis_forward='Y',
            axis_up='Z',
            bake_space_transform=False,
            use_custom_props=False,
            add_leaf_bones=False,
            bake_anim=False,
            path_mode='STRIP',
            embed_textures=False,
        )
    sidecar_of(path).write_text(digest + '\n', encoding='utf-8', newline='\n')


def write_prop_file(path, card):
    """The card's moving parts beside the FBX, in the card's own axes."""
    moving = []
    for entry in card.get('moving', []):
        moving.append({key: list(map(float, entry[key]))
                       if isinstance(entry[key], (tuple, list)) else entry[key]
                       for key in MOVING_KEYS if key in entry})
    document = {'name': card['name'], 'moving': moving}
    with open(prop_file_of(path), 'w', encoding='utf-8', newline='\n') as file:
        json.dump(document, file, indent=4)
        file.write('\n')
