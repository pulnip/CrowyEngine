"""Stacks pictures in one row: reference | still | approved, plus the same row 900 px wide.

Runs inside Blender's Python:

  blender -b --factory-startup --python-exit-code 1 --python .claude/skills/prop/tools/stack.py -- --out sheet.png [--thumb thumb.png] [--thumb-width 900] [--gap 16] a.png b.png [c.png]

Every picture is scaled to the smallest height among them; the gap is white.
"""

import os
import sys

import bpy
import numpy as np


def load(path):
    image = bpy.data.images.load(os.path.abspath(path), check_existing=False)
    return image


def to_array(image):
    width, height = image.size
    pixels = np.empty(width * height * 4, np.float32)
    image.pixels.foreach_get(pixels)
    return pixels.reshape(height, width, 4)[::-1].copy()


def from_array(name, pixels):
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(name, width, height, alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(pixels[::-1]).reshape(-1).astype(np.float32))
    return image


def save(image, path):
    image.filepath_raw = os.path.abspath(path)
    image.file_format = 'PNG'
    image.save()


def main(argv):
    out = None
    thumb = None
    thumb_width = 900
    gap = 16
    paths = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '--out':
            out = argv[i + 1]
            i += 2
        elif arg == '--thumb':
            thumb = argv[i + 1]
            i += 2
        elif arg == '--thumb-width':
            thumb_width = int(argv[i + 1])
            i += 2
        elif arg == '--gap':
            gap = int(argv[i + 1])
            i += 2
        else:
            paths.append(arg)
            i += 1
    if not out or not paths:
        raise SystemExit(__doc__)

    images = [load(path) for path in paths]
    height = min(image.size[1] for image in images)
    tiles = []
    for image in images:
        width = round(image.size[0] * height / image.size[1])
        image.scale(width, height)
        tile = to_array(image)
        tile[..., 3] = 1.0
        tiles.append(tile)
    spacer = np.ones((height, gap, 4), np.float32)
    row = tiles[0]
    for tile in tiles[1:]:
        row = np.concatenate([row, spacer, tile], axis=1)
    sheet = from_array('prop_stack_sheet', row)
    save(sheet, out)
    print(f"[stack] {row.shape[1]}x{row.shape[0]} -> {out}")
    if thumb:
        small = from_array('prop_stack_thumb', row)
        small.scale(thumb_width, round(row.shape[0] * thumb_width / row.shape[1]))
        save(small, thumb)
        print(f"[stack] {small.size[0]}x{small.size[1]} -> {thumb}")


if __name__ == '__main__':
    main(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
