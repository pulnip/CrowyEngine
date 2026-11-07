"""FloorLever: a floor lever on a stepped stone pedestal, the pilot prop."""
import math

AXLE = (0.0, 0.0, 0.45)
QUADRANT_RADIUS = 0.40
QUADRANT_SPAN = 50.0
NOTCH_ANGLES = (-35.0, -17.5, 0.0, 17.5, 35.0)
NOTCH_WIDTH = 0.035
NOTCH_DEPTH = 0.03
SLAB_TOP = 0.30
PLATE = (0.035, 0.065)

SPEC = {
    'name': 'FloorLever',
    'family': 'Interact',
    'purpose': 'a floor lever the player pulls between two stops; '
               'drives a gate',
    'size': {'width': 0.6, 'depth': 0.8, 'height': 1.2},
    'palette': ['Stone', 'Bronze', 'Iron', 'Glow'],
    'class': 'Near',
    'parts': [
        'stepped stone pedestal: three steps and a top slab with a slot '
        'under the quadrant',
        'two bronze quadrant plates with 5 notches along the arc',
        'bronze bearing blocks with rivets, the iron axle and its collars',
        'iron handle on a hub: pawl in the engaged notch, grip ring, knob',
        'glow in the engaged notch and one inlaid line along the rim',
    ],
    'moving': [
        {'name': 'Handle', 'kind': 'hinge', 'anchor': AXLE,
         'axis': (1.0, 0.0, 0.0), 'range': (-35.0, 35.0)},
    ],
    'tags': {'technique': 'kitbash', 'input': 'words'},
}


def arc_point(angle, radius):
    """(y, z) at `angle` degrees from straight up toward +Y, about the
    axle."""
    a = math.radians(angle)
    return (radius * math.sin(a), AXLE[2] + radius * math.cos(a))


def quadrant_profile():
    """The plate in the side view: a fan on a foot, notched along its arc."""
    half = math.degrees(NOTCH_WIDTH / 2 / QUADRANT_RADIUS)
    inner = QUADRANT_RADIUS - NOTCH_DEPTH
    steps = [QUADRANT_SPAN - 2.5 * k
             for k in range(int(2 * QUADRANT_SPAN / 2.5) + 1)]
    entries = [(a, [arc_point(a, QUADRANT_RADIUS)]) for a in steps
               if all(abs(a - n) > half + 0.5 for n in NOTCH_ANGLES)]
    for n in NOTCH_ANGLES:
        entries.append((n + half, [arc_point(n + half, QUADRANT_RADIUS),
                                   arc_point(n + half, inner)]))
        entries.append((n - half, [arc_point(n - half, inner),
                                   arc_point(n - half, QUADRANT_RADIUS)]))
    entries.sort(key=lambda entry: -entry[0])
    arc = [point for _, points in entries for point in points]
    return [(-0.10, SLAB_TOP), (0.10, SLAB_TOP)] + arc


def inlay_profile(radius, width, span):
    """An arc band in the side view, `width` across, centered on `radius`."""
    angles = [span - 3.0 * k for k in range(int(2 * span / 3.0) + 1)]
    outer = [arc_point(a, radius + width / 2) for a in angles]
    inner = [arc_point(a, radius - width / 2) for a in reversed(angles)]
    return outer + inner


def build_pedestal(kit):
    kit.add_block('Stone', (-0.30, 0.30), (-0.40, 0.40), (0.0, 0.10),
                  bevel=0.015)
    kit.add_block('Stone', (-0.27, 0.27), (-0.36, 0.36), (0.10, 0.18),
                  bevel=0.015)
    kit.add_block('Stone', (-0.24, 0.24), (-0.32, 0.32), (0.18, 0.25),
                  bevel=0.012)
    with kit.mirrored('x'):
        kit.add_block('Stone', (0.03, 0.26), (-0.34, 0.34),
                      (0.25, SLAB_TOP), bevel=0.01)
    with kit.mirrored('y'):
        kit.add_block('Stone', (-0.03, 0.03), (0.14, 0.34),
                      (0.25, SLAB_TOP), bevel=0.01)


def build_frame(kit):
    x, z = AXLE[0], AXLE[2]
    with kit.mirrored('x'):
        kit.add_extrusion('Bronze', quadrant_profile(), 'x', *PLATE)
        kit.add_block('Bronze', (PLATE[1], 0.10), (-0.07, 0.07),
                      (SLAB_TOP, 0.52), bevel=0.006)
        for y in (-0.045, 0.045):
            for rivet_z in (0.335, 0.495):
                kit.add_cylinder('Iron', (0.097, y, rivet_z),
                                 (0.106, y, rivet_z), 0.01, segments=12)
        kit.add_cylinder('Iron', (0.10, 0.0, z), (0.12, 0.0, z), 0.03,
                         segments=24)
        # the glow fills the bottom of the engaged notch
        kit.add_block('Glow', (PLATE[0] + 0.002, PLATE[1] - 0.002),
                      (-0.014, 0.014), (0.815, 0.83))
    kit.add_cylinder('Iron', (x - 0.15, 0.0, z), (x + 0.15, 0.0, z), 0.02,
                     segments=24)
    kit.add_extrusion('Glow', inlay_profile(0.33, 0.01, 42.0), 'x',
                      PLATE[1] - 0.001, PLATE[1] + 0.003)


def build_handle(kit):
    x, y, z = AXLE
    with kit.part('Handle', anchor=AXLE):
        kit.add_cylinder('Iron', (x - 0.03, y, z), (x + 0.03, y, z), 0.045,
                         segments=24)
        kit.add_cylinder('Iron', (x, y, z), (x, y, 1.12), 0.02,
                         segments=24)
        kit.add_block('Iron', (-0.015, 0.015), (-0.02, 0.02), (0.27, z),
                      bevel=0.004)
        kit.add_block('Iron', (-0.06, 0.06), (-0.012, 0.012),
                      (0.834, 0.862), bevel=0.003)
        kit.add_cylinder('Bronze', (x, y, 1.00), (x, y, 1.04), 0.026,
                         segments=24)
        kit.add_sphere('Iron', (x, y, 1.15), 0.05, segments=24, rings=12)


def build(kit):
    build_pedestal(kit)
    build_frame(kit)
    build_handle(kit)
