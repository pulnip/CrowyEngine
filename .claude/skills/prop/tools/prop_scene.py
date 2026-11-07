"""Writes a scene file that shows one prop to an orbiting camera.

  python .claude/skills/prop/tools/prop_scene.py --mesh Props/Interact/FloorLever.fbx --materials Stone,Bronze,Iron,Glow
      [--az 35] [--el 20] [--dist 3] [--target 0,0.6,0] [--fov 40] [--no-floor] --out captures/prop/FloorLever.json

The camera stands at the distance from the target, az degrees from the engine's -Z side toward +X,
el degrees above the floor, and looks at the target with no roll (the engine's lookRotation).
A material is a shop name (Props/Shop/<Name>.json) or a full asset ref.
"""

import argparse
import json
import math


def quat_x(radians):
    return (math.sin(radians / 2), 0.0, 0.0, math.cos(radians / 2))


def quat_y(radians):
    return (0.0, math.sin(radians / 2), 0.0, math.cos(radians / 2))


def multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def look_rotation(direction):
    x, y, z = direction
    yaw = math.atan2(x, z)
    pitch = math.asin(max(-1.0, min(1.0, -y)))
    return multiply(quat_y(yaw), quat_x(pitch))


def material_ref(name):
    if '/' in name or ':' in name:
        return name
    return f'Props/Shop/{name}.json'


def actor(actor_id, name, position, rotation, scale, components, parent=None):
    return {
        'id': actor_id,
        'name': name,
        'parent': parent,
        'transform': {
            'position': list(position),
            'rotation': [round(v, 7) for v in rotation],
            'scale': list(scale),
        },
        'components': components,
    }


def build(args):
    az = math.radians(args.az)
    el = math.radians(args.el)
    tx, ty, tz = (float(v) for v in args.target.split(','))
    offset = (
        args.dist * math.sin(az) * math.cos(el),
        args.dist * math.sin(el),
        -args.dist * math.cos(az) * math.cos(el),
    )
    position = (tx + offset[0], ty + offset[1], tz + offset[2])
    length = math.sqrt(sum(v * v for v in offset))
    direction = tuple(-v / length for v in offset)

    actors = [
        actor(1, 'camera', position, look_rotation(direction), (1.0, 1.0, 1.0), [
            {'type': 'CameraComponent', 'fovY': round(math.radians(args.fov), 7), 'nearZ': 0.05, 'farZ': 100.0},
        ]),
        actor(2, 'environment', (0.0, 0.0, 0.0), (0.5, 0.0, 0.0, 0.8660254), (1.0, 1.0, 1.0), [
            {
                'type': 'EnvironmentComponent',
                'skyAmbient': [0.123, 0.174, 0.262],
                'groundAmbient': [0.033, 0.024, 0.016],
                'skyColor': [0.139, 0.212, 0.356],
            },
            {
                'type': 'LightComponent',
                'kind': 'Directional',
                'enabled': True,
                'castShadow': True,
                'color': [1.0, 1.0, 1.0],
                'intensity': 3.0,
                'range': 10.0,
                'innerConeAngle': 0.0,
                'outerConeAngle': 0.785,
                'shadowBias': 0.02,
                'shadowNormalBias': 1.0,
            },
        ]),
    ]
    if not args.no_floor:
        actors.append(actor(3, 'floor', (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (10.0, 1.0, 10.0), [
            {'type': 'MeshComponent', 'mesh': 'engine:Builtin/Plane', 'materials': [], 'visible': True, 'castShadow': True},
        ]))
    actors.append(actor(4, 'prop', (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0), [
        {
            'type': 'MeshComponent',
            'mesh': args.mesh,
            'materials': [material_ref(name) for name in args.materials.split(',') if name],
            'visible': True,
            'castShadow': True,
        },
    ]))
    return {'metadata': {'version': 1, 'type': 'scene'}, 'actors': actors}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mesh', required=True)
    parser.add_argument('--materials', default='')
    parser.add_argument('--az', type=float, default=35.0)
    parser.add_argument('--el', type=float, default=20.0)
    parser.add_argument('--dist', type=float, default=3.0)
    parser.add_argument('--target', default='0,0.5,0')
    parser.add_argument('--fov', type=float, default=40.0)
    parser.add_argument('--no-floor', action='store_true')
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    with open(args.out, 'w', newline='\n', encoding='utf-8') as file:
        json.dump(build(args), file, indent=4)
        file.write('\n')
    print(f'[scene] {args.out}')


if __name__ == '__main__':
    main()
