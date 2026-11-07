"""Measures a picture of a prop: bounding box, fill, edge share, dark share.

Runs inside Blender's Python (numpy ships with it):

  blender -b --factory-startup --python-exit-code 1 --python .claude/skills/prop/tools/measure.py -- <image> [<image>] [--grid out.png] [--dark 80] [--edge 0.10]

The first image is the reference, the second yours; with two, the ratios yours / reference are
printed. --grid writes the first image with a 10 x 10 grid over its bounding box, to read
proportions off. The background is taken from the corners, or from alpha when there is any.
"""

import os
import sys

import bpy
import numpy as np


def load(path):
    image = bpy.data.images.load(os.path.abspath(path), check_existing=False)
    width, height = image.size
    pixels = np.empty(width * height * 4, np.float32)
    image.pixels.foreach_get(pixels)
    return pixels.reshape(height, width, 4)[::-1].copy()


def save(pixels, path):
    height, width = pixels.shape[:2]
    image = bpy.data.images.new('prop_measure_out', width, height, alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(pixels[::-1]).reshape(-1).astype(np.float32))
    image.filepath_raw = os.path.abspath(path)
    image.file_format = 'PNG'
    image.save()


def foreground(pixels):
    rgb = pixels[..., :3]
    alpha = pixels[..., 3]
    if alpha.min() < 0.5:
        return alpha > 0.5
    patch = 8
    corners = np.concatenate([
        rgb[:patch, :patch].reshape(-1, 3), rgb[:patch, -patch:].reshape(-1, 3),
        rgb[-patch:, :patch].reshape(-1, 3), rgb[-patch:, -patch:].reshape(-1, 3),
    ])
    background = np.median(corners, axis=0)
    return np.abs(rgb - background).max(axis=2) > 0.08


def analyse(pixels, dark, edge):
    rgb = pixels[..., :3]
    luma = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]
    mask = foreground(pixels)
    ys, xs = np.nonzero(mask)
    if len(ys) == 0:
        raise SystemExit('no foreground found: the picture is one flat colour')
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    box = luma[y0:y1, x0:x1]
    box_mask = mask[y0:y1, x0:x1]
    area = box_mask.sum()
    edges = np.zeros_like(box, dtype=bool)
    edges[:, 1:] |= np.abs(np.diff(box, axis=1)) > edge
    edges[1:, :] |= np.abs(np.diff(box, axis=0)) > edge
    return {
        'box': (int(x0), int(y0), int(x1 - x0), int(y1 - y0)),
        'aspect': (x1 - x0) / (y1 - y0),
        'fill': area / box_mask.size,
        'dark': ((box < dark / 255.0) & box_mask).sum() / area,
        'edge': (edges & box_mask).sum() / area,
    }


def draw_grid(pixels, box):
    out = pixels.copy()
    x0, y0, width, height = box
    for i in range(11):
        thick = 2 if i in (0, 5, 10) else 1
        x = min(x0 + round(i * width / 10), x0 + width - 1)
        y = min(y0 + round(i * height / 10), y0 + height - 1)
        out[y0:y0 + height, x:x + thick, :3] = (1.0, 0.0, 1.0)
        out[y:y + thick, x0:x0 + width, :3] = (1.0, 0.0, 1.0)
    return out


def main(argv):
    images = []
    grid = None
    dark = 80.0
    edge = 0.10
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '--grid':
            grid = argv[i + 1]
            i += 2
        elif arg == '--dark':
            dark = float(argv[i + 1])
            i += 2
        elif arg == '--edge':
            edge = float(argv[i + 1])
            i += 2
        else:
            images.append(arg)
            i += 1
    if not images:
        raise SystemExit(__doc__)

    results = []
    for path in images:
        pixels = load(path)
        result = analyse(pixels, dark, edge)
        results.append(result)
        x, y, w, h = result['box']
        print(f"[measure] {path}")
        print(f"  box x={x} y={y} w={w} h={h}  aspect w/h={result['aspect']:.3f}")
        print(f"  fill={result['fill']:.3f}  edge={result['edge']:.3f}  dark={result['dark']:.3f}")
        if grid and path == images[0]:
            save(draw_grid(pixels, result['box']), grid)
            print(f"  grid -> {grid}")
    if len(results) == 2:
        ref, mine = results
        print("[measure] yours / reference")
        print(f"  edge={mine['edge'] / max(ref['edge'], 1e-6):.3f}  dark={mine['dark'] / max(ref['dark'], 1e-6):.3f}  aspect diff={mine['aspect'] - ref['aspect']:+.3f}")


if __name__ == '__main__':
    main(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
