"""Blender library for the prop generator (Tools/gen.py).

Blender axes: Z up, a prop's front faces +Y and its right side +X, metres,
and its origin sits at the centre of its footprint on the ground. The FBX
keeps Blender's Z-up axes; the engine's loader converts them to its Y up.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CONTENT = ROOT / 'Content'
SHOP = CONTENT / 'Props' / 'Shop'
