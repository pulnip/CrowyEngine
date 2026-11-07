"""Coplanar overlaps of faces of different materials that show: z-fighting.

Two faces fight when their materials differ, their normals agree, their planes
lie within TOLERANCE of each other and they overlap by more than MIN_AREA.
Faces of one material shade alike, so their overlap does not show, and an
overlap buried in a solid does not show either: it counts only when some
sample point COVER in front of it lies outside every closed solid given. The
generator checks every prop as built, its parts at state 0.
"""
import math
from collections import defaultdict

TOLERANCE = 0.003
MIN_AREA = 1e-3
NORMAL_STEP = 1e-3
COVER = 0.005
GRID = 1.0
# A ray direction off every axis and diagonal, so it rarely grazes an edge.
RAY = (0.5772, 0.5777, 0.5774)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def newell_normal(points):
    """The polygon's unit normal and twice its area (Newell's method)."""
    n = [0.0, 0.0, 0.0]
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        n[0] += (a[1] - b[1]) * (a[2] + b[2])
        n[1] += (a[2] - b[2]) * (a[0] + b[0])
        n[2] += (a[0] - b[0]) * (a[1] + b[1])
    length = math.sqrt(dot(n, n))
    if length == 0.0:
        return None, 0.0
    return (n[0] / length, n[1] / length, n[2] / length), length


def plane_axes(normal):
    helper = (1.0, 0.0, 0.0) if abs(normal[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = cross(normal, helper)
    length = math.sqrt(dot(u, u))
    u = (u[0] / length, u[1] / length, u[2] / length)
    return u, cross(normal, u)


def triangles(points):
    """A fan; the kit's faces are convex or triangulated already."""
    return [(points[0], points[i], points[i + 1])
            for i in range(1, len(points) - 1)]


def clip(subject, edge_a, edge_b):
    """Sutherland-Hodgman: the part of subject left of edge a->b."""
    def inside(p):
        return ((edge_b[0] - edge_a[0]) * (p[1] - edge_a[1])
                - (edge_b[1] - edge_a[1]) * (p[0] - edge_a[0])) >= -1e-12

    def intersection(p, q):
        dx, dy = q[0] - p[0], q[1] - p[1]
        ex, ey = edge_b[0] - edge_a[0], edge_b[1] - edge_a[1]
        denominator = dx * ey - dy * ex
        if denominator == 0.0:
            return q
        t = ((edge_a[0] - p[0]) * ey - (edge_a[1] - p[1]) * ex) / denominator
        return (p[0] + t * dx, p[1] + t * dy)

    out = []
    for i, current in enumerate(subject):
        previous = subject[i - 1]
        if inside(current):
            if not inside(previous):
                out.append(intersection(previous, current))
            out.append(current)
        elif inside(previous):
            out.append(intersection(previous, current))
    return out


def area_2d(points):
    return 0.5 * abs(sum(points[i][0] * points[(i + 1) % len(points)][1]
                         - points[(i + 1) % len(points)][0] * points[i][1]
                         for i in range(len(points))))


def ccw(triangle):
    a, b, c = triangle
    if (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) < 0:
        return (a, c, b)
    return triangle


def overlap_regions(first, second):
    """The overlap of two lists of 2D triangles, as convex polygons."""
    regions = []
    for a in first:
        for b in second:
            region = list(a)
            b = ccw(b)
            for i in range(3):
                region = clip(region, b[i], b[(i + 1) % 3])
                if len(region) < 3:
                    break
            if len(region) >= 3 and area_2d(region) > 1e-9:
                regions.append(region)
    return regions


class Solid:
    """One closed surface as triangles, for inside tests by ray parity."""

    def __init__(self, polygons):
        self.triangles = [t for points in polygons for t in triangles(points)]
        coords = [p for t in self.triangles for p in t]
        self.low = tuple(min(p[k] for p in coords) for k in range(3))
        self.high = tuple(max(p[k] for p in coords) for k in range(3))

    def contains(self, point):
        if any(point[k] < self.low[k] or point[k] > self.high[k]
               for k in range(3)):
            return False
        crossings = 0
        for a, b, c in self.triangles:
            e1, e2 = sub(b, a), sub(c, a)
            p = cross(RAY, e2)
            det = dot(e1, p)
            if abs(det) < 1e-12:
                continue
            s = sub(point, a)
            u = dot(s, p) / det
            if u < 0.0 or u > 1.0:
                continue
            q = cross(s, e1)
            v = dot(RAY, q) / det
            if v < 0.0 or u + v > 1.0:
                continue
            if dot(e2, q) / det > 0.0:
                crossings += 1
        return crossings % 2 == 1


def solids_of(vertices, faces):
    """The connected groups of faces (each kit primitive, each voxel blob)
    as Solids. faces are vertex index tuples; vertices are points."""
    parent = list(range(len(vertices)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for indices in faces:
        for i in indices[1:]:
            parent[find(i)] = find(indices[0])
    groups = defaultdict(list)
    for indices in faces:
        groups[find(indices[0])].append([tuple(vertices[i]) for i in indices])
    return [Solid(polygons) for polygons in groups.values()]


class SolidIndex:
    """Solids bucketed by GRID cells of their bounding boxes."""

    def __init__(self, solids):
        self.cells = defaultdict(list)
        for solid in solids:
            lo = [math.floor(c / GRID) for c in solid.low]
            hi = [math.floor(c / GRID) for c in solid.high]
            for i in range(lo[0], hi[0] + 1):
                for j in range(lo[1], hi[1] + 1):
                    for k in range(lo[2], hi[2] + 1):
                        self.cells[(i, j, k)].append(solid)

    def covered(self, point):
        cell = tuple(math.floor(c / GRID) for c in point)
        return any(solid.contains(point) for solid in self.cells.get(cell, ()))


def samples(region_3d):
    """The region's centre and its corners pulled a fifth of the way in."""
    count = len(region_3d)
    centre = tuple(sum(p[k] for p in region_3d) / count for k in range(3))
    return [centre] + [tuple(p[k] + 0.2 * (centre[k] - p[k]) for k in range(3))
                       for p in region_3d]


def fights(faces, solids=(), tolerance=TOLERANCE):
    """Sites among (owner, material, points) faces: {(owner, owner): [area,
    centre, normal, material pairs]}, one per pair of owners (an owner may
    pair with itself). An overlap whose every sample is covered by one of
    `solids` (a point COVER in front of it lies inside) is left out;
    `tolerance` is how far apart two planes may lie and still fight."""
    index = SolidIndex(solids)
    planes = defaultdict(list)
    for owner, material, points in faces:
        points = [tuple(p) for p in points]
        normal, double_area = newell_normal(points)
        if normal is None or double_area / 2 < MIN_AREA:
            continue
        key = tuple(round(c / NORMAL_STEP) for c in normal)
        planes[key].append((dot(normal, points[0]), owner, normal, points,
                            material))

    sites = {}
    for group in planes.values():
        group.sort(key=lambda face: face[0])
        for i, (offset, owner, normal, points, material) in enumerate(group):
            u, v = plane_axes(normal)
            flat = [(dot(p, u), dot(p, v)) for p in points]
            box = (min(p[0] for p in flat), min(p[1] for p in flat),
                   max(p[0] for p in flat), max(p[1] for p in flat))
            for other in group[i + 1:]:
                if other[0] - offset > tolerance:
                    break
                if other[4] == material:
                    continue
                other_flat = [(dot(p, u), dot(p, v)) for p in other[3]]
                if (max(p[0] for p in other_flat) <= box[0]
                        or min(p[0] for p in other_flat) >= box[2]
                        or max(p[1] for p in other_flat) <= box[1]
                        or min(p[1] for p in other_flat) >= box[3]):
                    continue
                regions = overlap_regions(triangles(flat),
                                          triangles(other_flat))
                area = sum(area_2d(r) for r in regions)
                if area < MIN_AREA:
                    continue
                front = other[0] + COVER
                shown = 0.0
                for region in regions:
                    region_3d = [tuple(a * u[k] + b * v[k] + front * normal[k]
                                       for k in range(3)) for a, b in region]
                    if not all(index.covered(p) for p in samples(region_3d)):
                        shown += area_2d(region)
                if shown < MIN_AREA:
                    continue
                pair = tuple(sorted((owner, other[1])))
                centre = tuple(sum(p[k] for p in points) / len(points)
                               for k in range(3))
                site = sites.setdefault(pair, [0.0, centre, normal, set()])
                site[0] += shown
                site[3].add(tuple(sorted((material, other[4]))))
    return sites


def describe(area, centre, normal, materials):
    return (f'{area:.3f} m2 at ({centre[0]:.2f}, {centre[1]:.2f}, '
            f'{centre[2]:.2f}) normal ({normal[0]:.2f}, {normal[1]:.2f}, '
            f'{normal[2]:.2f}) materials {sorted(materials)}')
