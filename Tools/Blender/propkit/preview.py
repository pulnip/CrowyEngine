"""Preview sheet: six tiles for the modeler and the reviewer.

Top row: front, side and top, drawn flat with a 1 m grid and a 1.75 m figure
for scale. Bottom row: two shaded three-quarter close-ups of the asset alone
(from the front and from the back), and a tile of distance insets that shows
the asset at the pixel size a cut gives it from 15 m and from 50 m, each
beside a magnified copy. An asset too big for its inset at that pixel size
(a 9 m lamp at 15 m) is shown smaller, so it fits.

Workbench draws each material in its shop color (shop.preview_color, linear)
through the Standard view transform. The sheet is for the modeler's eyes
between runs; a grade comes from the engine's still.
"""
import math
import tempfile
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

from . import ROOT, shop
from .kit import MeshBuilder
from .spec import world_bounds

VIEW = 640
CELL = VIEW // 2
FIGURE_HEIGHT = 1.75
# Direction from the target to the camera; the asset's front faces +Y.
ORTHO_VIEWS = ((0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
CLOSE_VIEWS = ((1.0, 1.0, 0.8), (-1.0, -1.0, 0.8))
INSET_VIEW = (0.55, 1.0, 0.22)
# (pixels per metre, magnification): a 40 degree camera at 1080 px shows
# about 100 px per metre at 15 m and 30 px per metre at 50 m.
INSETS = ((100, 2), (30, 4))
BACKGROUND = (0.86, 0.88, 0.92)
SEPARATOR = 0.3
# The helpers' own materials and their linear colors.
FIGURE = 'PreviewFigure'
FLOOR = 'PreviewFloor'
GRID = 'PreviewGrid'
HELPER_COLORS = {FIGURE: (0.30, 0.18, 0.55), FLOOR: (0.62, 0.64, 0.66),
                 GRID: (0.25, 0.26, 0.28)}


def add_figure(builder, x, y):
    """A 1.75 m lathe silhouette: legs, torso, shoulders, neck, head."""
    profile = [(0.0, 0.12), (0.85, 0.16), (1.30, 0.21), (1.45, 0.18),
               (1.50, 0.06), (1.54, 0.06), (1.58, 0.10), (1.66, 0.11),
               (1.72, 0.08), (FIGURE_HEIGHT, 0.0)]
    builder.add_lathe(profile, FIGURE, segments=16, center=(x, y))


def add_floor(builder, low, high, line_width):
    x0, x1 = math.floor(low.x) - 1, math.ceil(high.x) + 1
    y0, y1 = math.floor(low.y) - 1, math.ceil(high.y) + 1
    builder.add_box(FLOOR, (x1 - x0, y1 - y0, 0.001),
                    ((x0 + x1) / 2, (y0 + y1) / 2, -0.0025))
    for x in range(x0, x1 + 1):
        builder.add_box(GRID, (line_width, y1 - y0, 0.0005),
                        (x, (y0 + y1) / 2, -0.00175))
    for y in range(y0, y1 + 1):
        builder.add_box(GRID, (x1 - x0, line_width, 0.0005),
                        ((x0 + x1) / 2, y, -0.00175))


def setup_render(scene):
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    scene.render.dither_intensity = 0.0
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.display.render_aa = '8'
    shading = scene.display.shading
    shading.color_type = 'MATERIAL'
    shading.show_cavity = True
    shading.cavity_type = 'WORLD'
    shading.show_object_outline = True
    shading.object_outline_color = (0.08, 0.07, 0.12)
    scene.view_settings.view_transform = 'Standard'
    world = bpy.data.worlds.new('PreviewWorld')
    world.color = BACKGROUND
    scene.world = world


def set_shading(scene, shaded):
    """Flat shows the exact shop colors; studio light shows the form."""
    shading = scene.display.shading
    shading.light = 'STUDIO' if shaded else 'FLAT'
    if shaded:
        shading.studio_light = 'studio.sl'
    shading.show_shadows = shaded
    shading.shadow_intensity = 0.25


def spread(corners, axis):
    values = [corner.dot(axis) for corner in corners]
    return max(values) - min(values)


def aim_camera(camera, target, direction, low, high, ortho_scale=None):
    """Point the orthographic camera at target; frame the box by default."""
    direction = Vector(direction).normalized()
    up = Vector((0.0, 1.0, 0.0)) if abs(direction.z) > 0.99 else Vector(
        (0.0, 0.0, 1.0))
    right = up.cross(direction).normalized()
    true_up = direction.cross(right)
    corners = [Vector((x, y, z)) for x in (low.x, high.x)
               for y in (low.y, high.y) for z in (low.z, high.z)]
    radius = (high - low).length
    if ortho_scale is None:
        ortho_scale = max(spread(corners, right),
                          spread(corners, true_up)) * 1.12
    camera.data.ortho_scale = ortho_scale
    camera.data.clip_end = radius * 4 + 10
    camera.location = target + direction * (radius * 2 + 5)
    if abs(direction.z) > 0.99:
        # Straight down with +X to the right and the front up the image.
        camera.rotation_euler = (0.0, 0.0, 0.0)
    else:
        camera.rotation_euler = (-direction).to_track_quat(
            '-Z', 'Y').to_euler()


def render(scene, size, path):
    """Render a square tile; returns float RGBA rows from the bottom up."""
    scene.render.resolution_x = size
    scene.render.resolution_y = size
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(scene.render.filepath)
    pixels = np.empty(size * size * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    bpy.data.images.remove(image)
    return pixels.reshape(size, size, 4)


def magnified(tile, factor):
    """The asset's pixels enlarged with hard edges, at most `factor` times,
    to fill the cell. The asset is whatever differs from the corner pixel."""
    size = tile.shape[0]
    background = tile[0, 0]
    mask = np.any(np.abs(tile[:, :, :3] - background[:3]) > 0.01, axis=2)
    rows, columns = np.nonzero(mask)
    if len(rows) == 0:
        rows = columns = np.array([size // 2])
    side = max(rows.max() - rows.min() + 1, columns.max() - columns.min() + 1,
               size // factor)
    scale = max(1, min(factor, size // side))
    side = size // scale
    top = int(np.clip((rows.min() + rows.max() + 1) // 2 - side // 2, 0,
                      size - side))
    left = int(np.clip((columns.min() + columns.max() + 1) // 2 - side // 2,
                       0, size - side))
    crop = tile[top:top + side, left:left + side]
    enlarged = np.repeat(np.repeat(crop, scale, axis=0), scale, axis=1)
    cell = np.empty_like(tile)
    cell[:] = background
    offset = (size - enlarged.shape[0]) // 2
    cell[offset:offset + enlarged.shape[0],
         offset:offset + enlarged.shape[1]] = enlarged
    return cell


def render_sheet(scene, asset_objects, materials, path):
    """materials: {shop name: material} of the prop; each takes its shop
    color here, after the export, so the FBX never carries it."""
    for name, material in materials.items():
        material.diffuse_color = (*shop.preview_color(name), 1.0)
    helpers = {}
    for name, color in HELPER_COLORS.items():
        helpers[name] = bpy.data.materials.new(name)
        helpers[name].diffuse_color = (*color, 1.0)

    low, high = world_bounds(asset_objects)
    # Behind and to the right, so neither the front nor the side view hides
    # the asset behind the figure.
    figure_x, figure_y = high.x + 0.6, low.y - 0.6
    view_low = Vector((low.x, figure_y - 0.25, 0.0))
    view_high = Vector((figure_x + 0.25, high.y,
                        max(high.z, FIGURE_HEIGHT)))
    figure_builder = MeshBuilder()
    add_figure(figure_builder, figure_x, figure_y)
    figure, = figure_builder.build(FIGURE, [FIGURE], helpers)
    floor_builder = MeshBuilder()
    line_width = max(0.01, (view_high - view_low).length / 500)
    add_floor(floor_builder, view_low, view_high, line_width)
    floor, = floor_builder.build(FLOOR, [FLOOR, GRID], helpers)

    setup_render(scene)
    camera_data = bpy.data.cameras.new('PreviewCamera')
    camera_data.type = 'ORTHO'
    camera = bpy.data.objects.new('PreviewCamera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    framed_center = (view_low + view_high) / 2
    asset_center = (low + high) / 2

    with tempfile.TemporaryDirectory() as temp:
        tile_path = Path(temp) / 'tile.png'

        set_shading(scene, shaded=False)
        orthos = []
        for direction in ORTHO_VIEWS:
            aim_camera(camera, framed_center, direction, view_low, view_high)
            orthos.append(render(scene, VIEW, tile_path))

        set_shading(scene, shaded=True)
        figure.hide_render = True
        closes = []
        for direction in CLOSE_VIEWS:
            aim_camera(camera, asset_center, direction, low, high)
            closes.append(render(scene, VIEW, tile_path))

        floor.hide_render = True
        inset_rows = []
        for px_per_m, factor in INSETS:
            aim_camera(camera, asset_center, INSET_VIEW, low, high)
            fitted = camera.data.ortho_scale
            aim_camera(camera, asset_center, INSET_VIEW, low, high,
                       ortho_scale=max(CELL / px_per_m, fitted))
            actual = render(scene, CELL, tile_path)
            inset_rows.append(np.concatenate(
                [actual, magnified(actual, factor)], axis=1))

    # Rows run from the bottom up, so the 50 m row goes under the 15 m row.
    insets = np.concatenate([inset_rows[1], inset_rows[0]], axis=0)
    insets[CELL - 1:CELL + 1, :, :3] = SEPARATOR
    insets[:, CELL - 1:CELL + 1, :3] = SEPARATOR
    sheet = np.concatenate([
        np.concatenate([closes[0], closes[1], insets], axis=1),
        np.concatenate(orthos, axis=1),
    ], axis=0)
    sheet[VIEW - 1:VIEW + 1, :, :3] = SEPARATOR
    for column in (VIEW, VIEW * 2):
        sheet[:, column - 1:column + 1, :3] = SEPARATOR

    path.parent.mkdir(parents=True, exist_ok=True)
    image = bpy.data.images.new('PreviewSheet', VIEW * 3, VIEW * 2,
                                alpha=False)
    image.pixels.foreach_set(sheet.ravel())
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()
    try:
        shown = path.relative_to(ROOT).as_posix()
    except ValueError:
        shown = str(path)
    print(f'[preview] {shown}')
