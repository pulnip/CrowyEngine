"""Node probe: three boxes in a parent chain for checking an importer's nodes.

Base holds Arm, turned a quarter around the up axis; Arm holds Tip, mirrored
along its own X. Base and Tip share Steel, Arm is Paint. The export settings
are Backlot's (Tools/Blender/backlot/export.py), which made AxisProbe.fbx.

Run from the repo root:

    blender --background --factory-startup --python-exit-code 1 \
        --python Engine/Content/Test/Data/NodeProbe.py -- \
        Engine/Content/Test/Data/NodeProbe.fbx
"""
import math
import sys

import bmesh
import bpy


def material(name, color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    bsdf = mat.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Base Color'].default_value = color
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    return mat


def box(name, size, center, mat):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for vert in bm.verts:
        vert.co = tuple(c * s + o for c, s, o in zip(vert.co, size, center))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def main():
    out = sys.argv[sys.argv.index('--') + 1]

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0

    steel = material('Steel', (0.6, 0.6, 0.65, 1.0), 1.0, 0.3)
    paint = material('Paint', (0.8, 0.2, 0.1, 1.0), 0.0, 0.6)

    base = box('Base', (0.4, 0.4, 0.1), (0.0, 0.0, 0.05), steel)

    arm = box('Arm', (0.3, 0.05, 0.05), (0.15, 0.0, 0.025), paint)
    arm.parent = base
    arm.location = (0.1, 0.0, 0.1)
    arm.rotation_euler = (0.0, 0.0, math.radians(90.0))

    tip = box('Tip', (0.1, 0.1, 0.1), (0.05, 0.0, 0.05), steel)
    tip.parent = arm
    tip.location = (0.3, 0.0, 0.0)
    tip.scale = (-1.0, 1.0, 1.0)

    bpy.ops.object.select_all(action='DESELECT')
    for obj in (base, arm, tip):
        obj.select_set(True)
    bpy.ops.export_scene.fbx(
        filepath=out,
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


main()
