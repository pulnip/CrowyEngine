"""Spec check: the card itself, the build against the card, then the built
objects against the card."""
from mathutils import Vector

from . import shop

AXES = ('width', 'depth', 'height')
CLASSES = ('Near', 'Mid', 'Far')
ORIGINS = ('center', 'base', 'free')
KINDS = ('hinge', 'slider', 'spin')
REQUIRED = ('name', 'family', 'purpose', 'size', 'palette', 'class', 'parts')
TRIANGLE_GUIDE = {'Near': 3000, 'Mid': 1500, 'Far': 500}
GROUND_TOLERANCE = 1e-4
ANCHOR_TOLERANCE = 1e-6


def world_bounds(objects):
    corners = [obj.matrix_world @ Vector(c)
               for obj in objects for c in obj.bound_box]
    low = Vector([min(c[i] for c in corners) for i in range(3)])
    high = Vector([max(c[i] for c in corners) for i in range(3)])
    return low, high


def is_point(value):
    return (isinstance(value, (tuple, list)) and len(value) == 3
            and all(isinstance(v, (int, float)) for v in value))


def check_moving(entry):
    """Failures in one entry of the card's `moving` list."""
    name = entry.get('name')
    if not isinstance(name, str) or not name:
        return [f'moving entry {entry!r} has no name']
    failures = []
    kind = entry.get('kind')
    if kind not in KINDS:
        failures.append(f'moving {name!r}: kind {kind!r} is not one of '
                        f'{KINDS}')
    if not is_point(entry.get('anchor')):
        failures.append(f'moving {name!r}: anchor is not (x, y, z)')
    axis = entry.get('axis')
    if not is_point(axis) or Vector(axis).length == 0.0:
        failures.append(f'moving {name!r}: axis is not a nonzero (x, y, z)')
    limits = entry.get('range')
    if kind == 'spin':
        if limits is not None:
            failures.append(f'moving {name!r}: a spin turns freely and has '
                            f'no range')
    elif (not isinstance(limits, (tuple, list)) or len(limits) != 2
          or not limits[0] < limits[1]):
        failures.append(f'moving {name!r}: range is not (low, high) with '
                        f'low < high')
    return failures


def check_card(card):
    """Failures in the card itself; an empty list when it is usable."""
    failures = [f'the card has no {key!r}' for key in REQUIRED
                if key not in card]
    if failures:
        return failures
    if card['class'] not in CLASSES:
        failures.append(f"class {card['class']!r} is not one of {CLASSES}")
    if card.get('origin', 'center') not in ORIGINS:
        failures.append(f"origin {card['origin']!r} is not one of {ORIGINS}")
    palette = card['palette']
    if not palette:
        failures.append('the card lists no palette names')
    if len(set(palette)) != len(palette):
        failures.append(f'the palette lists a name twice: {palette}')
    for name in palette:
        if not shop.has(name):
            failures.append(f'{name!r} is not in the shop '
                            f'(Content/Props/Shop/{name}.json)')
    names = []
    for entry in card.get('moving', []):
        failures += check_moving(entry)
        names.append(entry.get('name'))
    if len(set(names)) != len(names):
        failures.append(f'moving lists a name twice: {names}')
    if card['name'] in names:
        failures.append(f"moving {card['name']!r} has the prop's own name")
    return failures


def check_build(card, kit):
    """Failures of the kit's faces and parts against the card, before any
    object exists: every face's name and part must have a slot and a node
    to go to."""
    failures = []
    palette = card['palette']
    used = kit.materials_used()
    extra = sorted(used - set(palette))
    if extra:
        failures.append(f'faces use names the palette does not list: {extra}')
    unused = [name for name in palette if name not in used]
    if unused:
        failures.append(f'no face uses {unused}; the engine binds materials '
                        f'by slot index, so every later slot would shift')

    built = {part for _, _, _, part in kit.faces if part is not None}
    moving = {entry['name']: entry for entry in card.get('moving', [])}
    for name, entry in moving.items():
        if name not in built:
            failures.append(f"moving {name!r} is not built: no face inside "
                            f"kit.part({name!r}, ...)")
            continue
        anchor = kit.parts[name]
        if (anchor - Vector(entry['anchor'])).length > ANCHOR_TOLERANCE:
            failures.append(f"moving {name!r}: kit.part's anchor "
                            f"{tuple(round(v, 4) for v in anchor)} is not "
                            f"the card's {tuple(entry['anchor'])}")
    for name in kit.parts:
        if name not in moving:
            failures.append(f"part {name!r} is not in the card's moving "
                            f"list; a part that does not move belongs in "
                            f"the root mesh")
    return failures


def check(card, objects, kit):
    """Print the report; return a list of failures (empty when it passes).

    The card's `origin` says where the prop's origin sits: `center` (the
    default: on the ground, at the center of the footprint), `base` (on the
    ground, anywhere under the prop) or `free` (no check).
    """
    low, high = world_bounds(objects)
    size = high - low
    tolerance = card.get('tolerance', 0.10)
    origin = card.get('origin', 'center')
    failures = []
    print(f"[spec] {card['name']}")
    for i, axis in enumerate(AXES):
        want = card['size'][axis]
        error = (size[i] - want) / want
        flag = 'ok' if abs(error) <= tolerance else 'FAIL'
        print(f'[spec]   {axis:6} {size[i]:7.3f} m  spec {want:7.3f} m  '
              f'{error:+6.1%}  {flag}')
        if flag == 'FAIL':
            failures.append(f'{axis} {size[i]:.3f} m vs {want:.3f} m')

    if origin != 'free' and abs(low.z) > GROUND_TOLERANCE:
        failures.append(f'the lowest point is at z = {low.z:.4f}, not on '
                        f'the ground (z = 0)')
    if origin == 'center':
        for i, axis in enumerate('xy'):
            offset = (low[i] + high[i]) / 2
            if abs(offset) > 0.005 + 0.01 * size[i]:
                failures.append(
                    f'the footprint is off-center by {offset:+.3f} m in '
                    f"{axis}; center it or set the card's origin to 'base'")

    def triangles(obj):
        return sum(len(p.vertices) - 2 for p in obj.data.polygons)

    total = sum(triangles(obj) for obj in objects)
    print(f'[spec]   bounds min {tuple(round(v, 3) for v in low)} '
          f'max {tuple(round(v, 3) for v in high)}')
    print(f"[spec]   triangles {total}, slots {card['palette']}")
    for obj in objects[1:]:
        print(f'[spec]   part {obj.name} at '
              f'{tuple(round(v, 4) for v in kit.parts[obj.name])}, '
              f'{triangles(obj)} triangles')
    guide = TRIANGLE_GUIDE[card['class']]
    if total > guide * 1.5:
        print(f"[spec]   note: {total} triangles is well above the "
              f"{card['class']} guide of about {guide}")
    return failures
