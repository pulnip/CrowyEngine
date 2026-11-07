"""The shop: one engine material per name, Content/Props/Shop/<Name>.json.

The FBX carries only the names. The engine binds a material document to each
slot by index and never reads the FBX's own material values, so the Blender
materials keep their defaults and the shop's values reach only the preview.
"""
import json

import bpy

from . import SHOP


def path_of(name):
    return SHOP / f'{name}.json'


def has(name):
    return path_of(name).exists()


def materials(names):
    """{name: material}, one plain Blender material per shop name."""
    return {name: bpy.data.materials.new(name) for name in names}


def preview_color(name):
    """The color the preview sheet shows for a name: its albedo, or its
    emissive scaled to a peak of 1 when it glows. Linear RGB."""
    data = json.loads(path_of(name).read_text(encoding='utf-8'))['data']
    emissive = data.get('emissive', (0.0, 0.0, 0.0))
    peak = max(emissive)
    if peak > 0.0:
        return tuple(c / peak for c in emissive)
    return tuple(data['albedo'])
