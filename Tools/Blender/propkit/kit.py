"""Kitbash mesh builder: primitives with exact dimensions in metres.

Every face carries one material, a shop name (Content/Props/Shop/<Name>.json);
the card's palette lists them in the FBX's slot order.

Every primitive is a closed solid with outward-facing faces. Points are
(x, y, z) in metres: Z up, the prop's front toward +Y, its right side +X.
Boxes and blocks take an optional bevel that chamfers their edges, and
add_voxels meshes a grid of cells.
Inside `with kit.transformed(...)` shapes are placed in a moved, turned or
scaled frame; inside `with kit.mirrored(axis)` every shape is added twice,
once as written and once reflected. Inside `with kit.part(name, anchor)`
shapes go to a part of their own, an object whose origin is the anchor.
"""
import contextlib
import math

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

# (neighbour offset, quad corners counter-clockwise seen from outside)
CUBE_FACES = (
    ((1, 0, 0), ((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1))),
    ((-1, 0, 0), ((0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0))),
    ((0, 1, 0), ((0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0))),
    ((0, -1, 0), ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1))),
    ((0, 0, 1), ((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1))),
    ((0, 0, -1), ((0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0))),
)
AXES = {'x': 0, 'y': 1, 'z': 2}
# The two profile coordinates of an extrusion along each axis, and whether
# (u, v, axis) is a right-handed frame.
PROFILE_AXES = {'x': ((1, 2), True), 'y': ((0, 2), False),
                'z': ((0, 1), True)}
# A pipe joint whose mitre would stretch the section more than this many
# times (a turn of about 150 degrees) is refused.
MAX_MITRE = 4.0
EPSILON = 1e-9
SAME_POINT = 1e-6


def perpendicular_frame(axis, up=None):
    """Unit vectors (u, v) with (u, v, axis) right-handed.

    u follows `up` (default +Z, or +Y when the axis is vertical) as far as it
    is perpendicular to the axis.
    """
    w = Vector(axis)
    if w.length < EPSILON:
        raise ValueError('the start and end points are the same')
    w.normalize()
    if up is None:
        up = (0.0, 1.0, 0.0) if abs(w.z) > 1.0 - 1e-6 else (0.0, 0.0, 1.0)
    up = Vector(up)
    u = up - up.dot(w) * w
    if u.length < EPSILON:
        raise ValueError(f'up {tuple(up)} is parallel to the axis')
    u.normalize()
    return u, w.cross(u)


def without_repeats(points, closed=False):
    """The points without consecutive duplicates (nor a closing repeat)."""
    kept = [p for i, p in enumerate(points)
            if i == 0 or (p - points[i - 1]).length > SAME_POINT]
    if closed and len(kept) > 1 and (kept[0] - kept[-1]).length < SAME_POINT:
        kept.pop()
    return kept


def round_corners(points, radius, segments=4, closed=False):
    """The polyline with its corners replaced by circular arcs.

    For pipes that bend, such as a lamp arm. radius is the bend radius of
    the centerline: keep it larger than the pipe's radius, or the inside of
    the bend folds through itself. A corner may use half of a segment it
    shares with another corner and all of an end segment; where that is too
    short, its radius shrinks. closed=True also rounds the corners at the
    first and last points of a loop; pass the result to
    add_pipe(..., closed=True).
    """
    points = without_repeats([Vector(p) for p in points], closed)
    count = len(points)
    if count < 3:
        return [tuple(p) for p in points]
    result = [] if closed else [points[0]]

    def append(point):
        if not result or (point - result[-1]).length > SAME_POINT:
            result.append(point)

    for i in (range(count) if closed else range(1, count - 1)):
        previous, corner = points[i - 1], points[i]
        following = points[(i + 1) % count]
        into, out = corner - previous, following - corner
        turn = into.angle(out, 0.0)
        if turn < 1e-4 or math.pi - turn < 1e-4:
            append(corner)
            continue
        share_in = 1.0 if not closed and i == 1 else 0.5
        share_out = 1.0 if not closed and i == count - 2 else 0.5
        cut = min(radius * math.tan(turn / 2), into.length * share_in,
                  out.length * share_out)
        into, out = into.normalized(), out.normalized()
        start, end = corner - into * cut, corner + out * cut
        axis = into.cross(out).normalized()
        center = start + axis.cross(into) * (cut / math.tan(turn / 2))
        append(start)
        for k in range(1, segments):
            append(center + Matrix.Rotation(turn * k / segments, 3, axis)
                   @ (start - center))
        append(end)
    if not closed:
        append(points[-1])
    return [tuple(p) for p in result]


def signed_area(points):
    return sum(a[0] * b[1] - b[0] * a[1]
               for a, b in zip(points, points[1:] + points[:1])) / 2


def is_convex(points):
    """True for a counter-clockwise polygon with no reflex corner."""
    count = len(points)
    for i in range(count):
        a, b, c = points[i - 1], points[i], points[(i + 1) % count]
        if (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]) \
                < -EPSILON:
            return False
    return True


def triangulate(points):
    """Ear clipping of a simple counter-clockwise polygon; index triples."""

    def cross(a, b, c):
        return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])

    def inside(p, a, b, c):
        return (cross(a, b, p) >= -EPSILON and cross(b, c, p) >= -EPSILON
                and cross(c, a, p) >= -EPSILON)

    remaining = list(range(len(points)))
    triangles = []
    while len(remaining) > 3:
        for j in range(len(remaining)):
            i0 = remaining[j - 1]
            i1 = remaining[j]
            i2 = remaining[(j + 1) % len(remaining)]
            a, b, c = points[i0], points[i1], points[i2]
            if cross(a, b, c) <= EPSILON:
                continue
            if any(inside(points[k], a, b, c) for k in remaining
                   if k not in (i0, i1, i2) and points[k] not in (a, b, c)):
                continue
            triangles.append((i0, i1, i2))
            del remaining[j]
            break
        else:
            raise ValueError('the profile is not a simple polygon')
    triangles.append(tuple(remaining))
    return triangles


class MeshBuilder:
    """Faces are (vertex indices, material, smooth, part); part is None for
    the root mesh. parts maps each part's name to its anchor."""

    def __init__(self):
        self.vertices = []
        self.faces = []
        self.parts = {}
        self._frames = []
        self._part = None
        self._mirrors = 0

    round_corners = staticmethod(round_corners)

    # ----- frames -----

    @property
    def frame(self):
        """The current local-to-asset matrix."""
        return self._frames[-1] if self._frames else Matrix.Identity(4)

    @contextlib.contextmanager
    def part(self, name, anchor):
        """Shapes added in the block become the part `name`: its own object,
        a child of the root, with its origin at `anchor`.

        Shapes are written where they sit in the assembled prop, as outside
        the block; the anchor is a point in the current frame and stays put
        however the shapes inside turn. A name may be entered again with the
        same anchor. A part inside mirrored() would take its reflection
        around its own anchor, so mirror inside the part instead.
        """
        if self._part is not None:
            raise ValueError(f'part {name!r} inside part {self._part!r}')
        if self._mirrors:
            raise ValueError(f'part {name!r} inside mirrored(): build each '
                             f'side as its own part, or mirror inside it')
        anchor = self.frame @ Vector(anchor)
        known = self.parts.setdefault(name, anchor)
        if (known - anchor).length > SAME_POINT:
            raise ValueError(f'part {name!r} entered again with anchor '
                             f'{tuple(anchor)}, not {tuple(known)}')
        self._part = name
        try:
            yield
        finally:
            self._part = None

    @contextlib.contextmanager
    def transformed(self, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0),
                    scale=1.0):
        """Shapes added in the block are scaled, rotated, then moved.

        rotation: degrees about X, then Y, then Z. A positive angle turns
        counter-clockwise seen from the positive end of its axis: +X tips the
        front (+Y) up, +Y tips the right side (+X) down, +Z turns +X toward
        +Y. scale: a number or (x, y, z); a negative component mirrors, and
        the faces stay outward. Blocks nest.
        """
        if isinstance(scale, (int, float)):
            scale = (scale, scale, scale)
        local = (Matrix.Translation(location)
                 @ Euler([math.radians(a) for a in rotation],
                         'XYZ').to_matrix().to_4x4()
                 @ Matrix.Diagonal((*scale, 1.0)))
        self._frames.append(self.frame @ local)
        try:
            yield
        finally:
            self._frames.pop()

    @contextlib.contextmanager
    def mirrored(self, axis='x'):
        """Every shape added in the block is added again, reflected.

        The mirror plane passes through the current frame's origin across
        `axis` ('x' reflects left and right). A shape that crosses the plane
        would overlap its own reflection, so keep centered shapes outside the
        block. Blocks nest: 'x' inside 'y' gives four copies.
        """
        first_face = len(self.faces)
        frame = self.frame
        self._mirrors += 1
        try:
            yield
        finally:
            self._mirrors -= 1
        flip = [1.0, 1.0, 1.0, 1.0]
        flip[AXES[axis]] = -1.0
        reflection = frame @ Matrix.Diagonal(flip) @ frame.inverted()
        copies = {}
        for indices, material, smooth, part in self.faces[first_face:]:
            mirrored = []
            for i in indices:
                if i not in copies:
                    copies[i] = len(self.vertices)
                    self.vertices.append(reflection @ self.vertices[i])
                mirrored.append(copies[i])
            self.faces.append(
                (tuple(reversed(mirrored)), material, smooth, part))

    # ----- raw geometry -----

    def add_vertex(self, co):
        co = Vector(co)
        if self._frames:
            co = self._frames[-1] @ co
        self.vertices.append(co)
        return len(self.vertices) - 1

    def add_face(self, indices, material, smooth=False):
        """A polygon over existing vertices, counter-clockwise from outside."""
        indices = tuple(indices)
        if self._frames and self._frames[-1].is_negative:
            indices = indices[::-1]
        self.faces.append((indices, material, smooth, self._part))

    def add_ring(self, center, u, v, radius, segments):
        """Vertices on a circle in the plane of u and v, counter-clockwise
        about u x v."""
        center, u, v = Vector(center), Vector(u), Vector(v)
        return [self.add_vertex(center + radius * (math.cos(a) * u
                                                   + math.sin(a) * v))
                for a in (2 * math.pi * k / segments
                          for k in range(segments))]

    def bridge(self, lower, upper, material, smooth=True):
        """Quads between two rings of equal length (lower to upper along
        their axis), facing out."""
        count = len(lower)
        for k in range(count):
            k1 = (k + 1) % count
            quad = [lower[k], lower[k1], upper[k1], upper[k]]
            unique = list(dict.fromkeys(quad))
            if len(unique) >= 3:
                self.add_face(unique, material, smooth=smooth)

    # ----- primitives -----

    def add_box(self, material, size, center=(0.0, 0.0, 0.0), bevel=0.0):
        """Axis-aligned box; size is (x, y, z), center its middle.

        bevel > 0 cuts all twelve edges at 45 degrees, that wide on each
        face, so they catch the light (trims, sills, posts, machines). It
        must stay under half the smallest side.
        """
        assert min(size) > 0, \
            'add_box(material, size, center): size comes first'
        if bevel > 0.0:
            self._add_chamfered_box(material, size, center, bevel)
            return
        half = Vector(size) / 2
        origin = Vector(center) - half
        corner_index = {}
        for _, corners in CUBE_FACES:
            for corner in corners:
                if corner not in corner_index:
                    co = origin + Vector(
                        (corner[0] * size[0], corner[1] * size[1],
                         corner[2] * size[2]))
                    corner_index[corner] = self.add_vertex(co)
        for _, corners in CUBE_FACES:
            self.add_face([corner_index[c] for c in corners], material)

    def add_block(self, material, x, y, z, bevel=0.0):
        """Axis-aligned box given by its (low, high) limits on each axis.

        For panels and tiling modules built from exact edges: add_block(c,
        (-1.5, 1.5), (-0.3, 0.0), (0.0, 4.5)) is a 3 m wall whose front face
        lies at y = 0. bevel as for add_box; a bevelled block no longer fills
        its limits at the edges, so keep bevels off faces that butt a
        neighbour.
        """
        (x0, x1), (y0, y1), (z0, z1) = x, y, z
        assert x0 < x1 and y0 < y1 and z0 < z1, 'each limit is (low, high)'
        self.add_box(material, (x1 - x0, y1 - y0, z1 - z0),
                     ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), bevel)

    def _add_chamfered_box(self, material, size, center, bevel):
        """26 faces: six inset sides, twelve edge strips, eight corners."""
        assert bevel < min(size) / 2, 'the bevel must be under half a side'
        half = [s / 2 for s in size]
        center = Vector(center)
        local = {}

        def corner_vertex(signs, axis):
            """The corner `signs` as cut back on the side across `axis`."""
            key = (signs, axis)
            if key not in local:
                local[key] = Vector([
                    signs[k] * (half[k] - (0.0 if k == axis else bevel))
                    for k in range(3)])
            return key

        corners = [(sx, sy, sz) for sx in (-1, 1) for sy in (-1, 1)
                   for sz in (-1, 1)]
        polygons = []
        for axis in range(3):
            for side in (-1, 1):
                outward = [0.0, 0.0, 0.0]
                outward[axis] = float(side)
                polygons.append(([corner_vertex(c, axis) for c in corners
                                  if c[axis] == side], outward))
        for axis in range(3):
            b, c = [k for k in range(3) if k != axis]
            for sb in (-1, 1):
                for sc in (-1, 1):
                    ends = []
                    for sa in (-1, 1):
                        signs = [0, 0, 0]
                        signs[axis], signs[b], signs[c] = sa, sb, sc
                        ends.append(tuple(signs))
                    outward = [0.0, 0.0, 0.0]
                    outward[b], outward[c] = float(sb), float(sc)
                    polygons.append(([corner_vertex(e, k) for e in ends
                                      for k in (b, c)], outward))
        for signs in corners:
            polygons.append(([corner_vertex(signs, k) for k in range(3)],
                             [float(v) for v in signs]))

        index = {key: self.add_vertex(center + co)
                 for key, co in local.items()}
        for keys, outward in polygons:
            normal = Vector(outward).normalized()
            middle = sum((local[k] for k in keys), Vector()) / len(keys)
            u = (local[keys[0]] - middle).normalized()
            v = normal.cross(u)
            keys = sorted(keys, key=lambda k: math.atan2(
                (local[k] - middle).dot(v), (local[k] - middle).dot(u)))
            self.add_face([index[k] for k in keys], material)

    def add_voxels(self, cells, size, origin=(0.0, 0.0, 0.0)):
        """A grid of cubes, `size` on a side, with the faces between two
        filled cells left out.

        cells maps (i, j, k) to a material; cell (i, j, k) spans origin + size
        * (i..i+1, j..j+1, k..k+1). Corners are shared, so the result is one
        closed surface per connected group of cells.
        """
        origin = Vector(origin)
        lattice = {}

        def point(p):
            if p not in lattice:
                lattice[p] = self.add_vertex(origin + size * Vector(p))
            return lattice[p]

        for (i, j, k), material in sorted(cells.items()):
            for (di, dj, dk), corners in CUBE_FACES:
                if (i + di, j + dj, k + dk) in cells:
                    continue
                self.add_face([point((i + a, j + b, k + c))
                               for a, b, c in corners], material)

    def add_beam(self, material, start, end, size, up=None):
        """A box from start to end with a (width, height) cross-section.

        For struts, braces, legs and rails. The height runs along `up`
        (default +Z; +Y for a vertical beam) and the width across it; for
        splayed legs pass `up` (say, each leg's outward direction) so every
        section turns the same way. The section is centered on the start-end
        line and both ends are cut square to it, so a leg leaning by angle a
        that ends at z = 0 dips below the ground by about height / 2 * sin(a):
        start it that much higher or stand it on a foot.
        """
        start, end = Vector(start), Vector(end)
        axis = end - start
        height_axis, v = perpendicular_frame(axis, up)
        # (width, height, axis) must be right-handed for CUBE_FACES.
        width_axis = -v
        width, height = size
        corner_index = {}
        for _, corners in CUBE_FACES:
            for corner in corners:
                if corner not in corner_index:
                    co = (start + (corner[0] - 0.5) * width * width_axis
                          + (corner[1] - 0.5) * height * height_axis
                          + corner[2] * axis)
                    corner_index[corner] = self.add_vertex(co)
        for _, corners in CUBE_FACES:
            self.add_face([corner_index[c] for c in corners], material)

    def add_lathe(self, profile, materials, segments, center=(0.0, 0.0),
                  cap_bottom=True, cap_top=True):
        """Surface of revolution about a vertical axis.

        profile: (z, radius) points from bottom to top. materials: one per span
        between consecutive points, or a single name for all. The sides are
        smooth; the caps are flat. A radius of 0 closes the surface to a point.
        """
        spans = len(profile) - 1
        if isinstance(materials, str):
            materials = [materials] * spans
        assert len(materials) == spans, 'one material per profile span'
        cx, cy = center
        rings = []
        for z, radius in profile:
            if radius == 0:
                rings.append([self.add_vertex((cx, cy, z))] * segments)
                continue
            rings.append([
                self.add_vertex((cx + radius * math.cos(a),
                                 cy + radius * math.sin(a), z))
                for a in (2 * math.pi * k / segments
                          for k in range(segments))])
        for i, material in enumerate(materials):
            lower, upper = rings[i], rings[i + 1]
            for k in range(segments):
                k1 = (k + 1) % segments
                quad = [lower[k], lower[k1], upper[k1], upper[k]]
                unique = list(dict.fromkeys(quad))
                if len(unique) >= 3:
                    self.add_face(unique, material, smooth=True)
        if cap_bottom and profile[0][1] > 0:
            self.add_face(list(reversed(rings[0])), materials[0])
        if cap_top and profile[-1][1] > 0:
            self.add_face(rings[-1], materials[-1])

    def add_sphere(self, material, center, radius, segments=16, rings=8):
        """A UV sphere; `rings` bands from pole to pole."""
        cx, cy, cz = center
        profile = []
        for i in range(rings + 1):
            angle = math.pi * i / rings - math.pi / 2
            r = 0.0 if i in (0, rings) else radius * math.cos(angle)
            profile.append((cz + radius * math.sin(angle), r))
        self.add_lathe(profile, material, segments, center=(cx, cy))

    def add_cylinder(self, material, start, end, radius, segments=12,
                     end_radius=None, cap_start=True, cap_end=True,
                     cap_material=None):
        """A round rod from start to end, at any angle.

        end_radius makes it a cone frustum; a radius of 0 closes that end to
        a point. The side is smooth, the caps flat discs square to the axis,
        in cap_material when given. A rod leaning by angle a that ends at z = 0
        dips below the ground by radius * sin(a): lift it by that much.
        """
        start, end = Vector(start), Vector(end)
        u, v = perpendicular_frame(end - start)
        end_radius = radius if end_radius is None else end_radius
        cap_material = cap_material or material

        def ring(center, r):
            if r == 0:
                return [self.add_vertex(center)] * segments
            return self.add_ring(center, u, v, r, segments)

        lower, upper = ring(start, radius), ring(end, end_radius)
        self.bridge(lower, upper, material)
        if cap_start and radius > 0:
            self.add_face(list(reversed(lower)), cap_material)
        if cap_end and end_radius > 0:
            self.add_face(upper, cap_material)

    def add_tube(self, material, start, end, outer_radius, inner_radius,
                 segments=12):
        """A hollow cylinder from start to end with flat ring ends."""
        start, end = Vector(start), Vector(end)
        assert 0 < inner_radius < outer_radius, 'inner < outer radius'
        u, v = perpendicular_frame(end - start)
        outer_low = self.add_ring(start, u, v, outer_radius, segments)
        outer_high = self.add_ring(end, u, v, outer_radius, segments)
        inner_low = self.add_ring(start, u, v, inner_radius, segments)
        inner_high = self.add_ring(end, u, v, inner_radius, segments)
        self.bridge(outer_low, outer_high, material)
        # The inner wall faces the axis: bridge it downward.
        self.bridge(inner_high, inner_low, material)
        self.bridge(outer_high, inner_high, material, smooth=False)
        self.bridge(inner_low, outer_low, material, smooth=False)

    def add_pipe(self, material, points, radius, segments=8, closed=False,
                 cap_material=None):
        """A round pipe along a polyline, mitred at the joints.

        A joint that turns more than about 150 degrees raises ValueError:
        round it with round_corners() (segments=8 for a hairpin) or build two
        pipes. closed=True joins the last point back to the first (a ring);
        otherwise the ends are capped.
        """
        points = without_repeats([Vector(p) for p in points], closed)
        count = len(points)
        assert count >= (3 if closed else 2), 'too few points for a pipe'
        spans = count if closed else count - 1
        directions = [(points[(i + 1) % count] - points[i]).normalized()
                      for i in range(spans)]

        # Parallel transport keeps the side faces from twisting.
        frames = [perpendicular_frame(directions[0])[0]]
        for previous, following in zip(directions, directions[1:]):
            turn = previous.rotation_difference(following)
            frames.append((turn @ frames[-1]).normalized())
        if closed:
            # Spread the twist that remains after a full loop over the spans.
            back = (directions[-1].rotation_difference(directions[0])
                    @ frames[-1])
            twist = math.atan2(frames[0].cross(back).dot(directions[0]),
                               frames[0].dot(back))
            frames = [
                Matrix.Rotation(-twist * i / spans, 3, directions[i])
                @ frames[i] for i in range(spans)]

        def ring_at(i):
            """The joint ring of point i, seen from the span arriving at it."""
            if not closed and i == 0:
                incoming, outgoing, frame = directions[0], directions[0], \
                    frames[0]
            elif not closed and i == count - 1:
                incoming, outgoing, frame = directions[-1], directions[-1], \
                    frames[-1]
            else:
                incoming = directions[i - 1]
                outgoing = directions[i % spans]
                frame = frames[i - 1]
            u = frame
            v = incoming.cross(u)
            bisector = incoming + outgoing
            # The length is 2 cos(turn / 2); a sharper joint would flatten
            # the outgoing span.
            if bisector.length < 2.0 / MAX_MITRE:
                raise ValueError(
                    f'pipe joint {i} at {tuple(points[i])} turns more than '
                    f'about 150 degrees; round it with round_corners() or '
                    f'build two pipes')
            normal = bisector.normalized()
            along = incoming.dot(normal)
            ring = []
            for k in range(segments):
                angle = 2 * math.pi * k / segments
                offset = radius * (math.cos(angle) * u + math.sin(angle) * v)
                shift = -offset.dot(normal) / along
                ring.append(self.add_vertex(points[i] + offset
                                            + shift * incoming))
            return ring

        rings = [ring_at(i) for i in range(count)]
        for i in range(spans):
            self.bridge(rings[i], rings[(i + 1) % count], material)
        if not closed:
            cap_material = cap_material or material
            self.add_face(list(reversed(rings[0])), cap_material)
            self.add_face(rings[-1], cap_material)

    def add_extrusion(self, material, profile, axis, start, end,
                      cap_material=None):
        """A prism: a 2D profile swept straight along an axis.

        profile: the corners of a simple polygon, in either winding. Along
        'x' they are (y, z) points, as seen in the side view (+Y to the
        right); along 'y' (x, z), with +x the asset's right side, so the
        front view shows the profile mirrored; along 'z' (x, y), as seen
        from above. start and end are numbers: where the prism begins and
        ends on that axis. Concave profiles are fine (an L, a cornice
        moulding); their end faces are split into triangles.
        """
        (iu, iv), right_handed = PROFILE_AXES[axis]
        iw = AXES[axis]
        start, end = float(start), float(end)
        profile = [tuple(map(float, p)) for p in profile]
        profile = [p for i, p in enumerate(profile) if p != profile[i - 1]]
        if signed_area(profile) < 0:
            profile.reverse()
        if start > end:
            start, end = end, start
        cap_material = cap_material or material
        first_face = len(self.faces)

        def point(p, w):
            co = [0.0, 0.0, 0.0]
            co[iu], co[iv], co[iw] = p[0], p[1], w
            return co

        low = [self.add_vertex(point(p, start)) for p in profile]
        high = [self.add_vertex(point(p, end)) for p in profile]
        self.bridge(low, high, material, smooth=False)
        if is_convex(profile):
            caps = [tuple(range(len(profile)))]
        else:
            caps = triangulate(profile)
        for cap in caps:
            self.add_face([high[i] for i in cap], cap_material)
            self.add_face([low[i] for i in reversed(cap)], cap_material)
        if not right_handed:
            self.faces[first_face:] = [
                (tuple(reversed(indices)), material, smooth, part)
                for indices, material, smooth, part
                in self.faces[first_face:]]

    # ----- output -----

    def materials_used(self):
        return {material for _, material, _, _ in self.faces}

    def build(self, name, slots, materials):
        """The root mesh object `name`, and one object per part as its child.

        Every object takes the names in `slots` as its material slots, in
        that order, so the FBX lists them in that order whichever object it
        writes first; every face's material must be one of them. A part's
        vertices move by minus its anchor and its object stands at the
        anchor, so it sits where it was built with its origin on the anchor.
        """
        root = self._mesh_object(name, None, Vector(), slots, materials)
        objects = [root]
        for part, anchor in self.parts.items():
            obj = self._mesh_object(part, part, anchor, slots, materials)
            obj.parent = root
            obj.location = anchor
            objects.append(obj)
        # matrix_world follows a new parent and location only after this
        bpy.context.view_layer.update()
        return objects

    def _mesh_object(self, name, part, origin, slots, materials):
        bm = bmesh.new()
        verts = {}
        for indices, material, smooth, owner in self.faces:
            if owner != part:
                continue
            for i in indices:
                if i not in verts:
                    verts[i] = bm.verts.new(self.vertices[i] - origin)
            face = bm.faces.new([verts[i] for i in indices])
            face.smooth = smooth
            face.material_index = slots.index(material)
        for edge in bm.edges:
            edge.smooth = all(face.smooth for face in edge.link_faces)
        bm.normal_update()

        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        for slot in slots:
            mesh.materials.append(materials[slot])
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj
