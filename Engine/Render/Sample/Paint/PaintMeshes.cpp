#include "PaintMeshes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace Crowy
{
    namespace
    {
        // a flat convex polygon as a triangle fan, wound to face `normal`
        template<usize N>
        void addFan(
            PaintMeshTriangles& mesh,
            std::array<DVec3, N> corners,
            DVec3 normal
        ) {
            const auto base = static_cast<u32>(mesh.positions.size());
            const auto facing =
                dot(cross(corners[1] - corners[0], corners[2] - corners[0]),
                    normal);
            if(facing < 0.0)
                std::reverse(corners.begin() + 1, corners.end());

            for(const auto& corner: corners) {
                mesh.positions.push_back(toVec3(corner));
                mesh.normals.push_back(toVec3(normal));
            }
            for(u32 i = 1; i + 1 < N; ++i)
                mesh.indices.insert(
                    mesh.indices.end(),
                    {base, base + i, base + i + 1}
                );
        }
    }

    Box3d boundsOf(const PaintMeshTriangles& mesh) {
        Box3d bounds;
        for(const auto& p: mesh.positions)
            bounds.Add(toDVec3(p));

        return bounds;
    }

    PaintMeshTriangles makePaintCube(f64 size) {
        const auto h = 0.5 * size;
        PaintMeshTriangles mesh;

        for(usize axis = 0; axis < 3; ++axis) {
            const auto b = (axis + 1) % 3;
            const auto c = (axis + 2) % 3;
            for(const auto sign: {1.0, -1.0}) {
                DVec3 normal;
                normal[axis] = sign;

                std::array<DVec3, 4> corners;
                constexpr std::array<std::pair<f64, f64>, 4> Steps{
                    {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}}
                };
                for(usize i = 0; i < 4; ++i) {
                    corners[i][axis] = sign * h;
                    corners[i][b] = Steps[i].first * h;
                    corners[i][c] = Steps[i].second * h;
                }
                addFan(mesh, corners, normal);
            }
        }

        return mesh;
    }

    PaintMeshTriangles makePaintWedge(DVec3 size) {
        const auto x0 = -0.5 * size.x;
        const auto x1 = 0.5 * size.x;
        const auto y0 = -0.5 * size.y;
        const auto y1 = 0.5 * size.y;
        const auto top = size.z;
        PaintMeshTriangles mesh;

        // the slope, the floor, the high end and the two sides
        const auto slope = getSafeNormal(DVec3{-top, 0.0, size.x});
        addFan(
            mesh,
            std::array{
                DVec3{x0, y0, 0.0},
                DVec3{x1, y0, top},
                DVec3{x1, y1, top},
                DVec3{x0, y1, 0.0}
            },
            slope
        );
        addFan(
            mesh,
            std::array{
                DVec3{x0, y0, 0.0},
                DVec3{x1, y0, 0.0},
                DVec3{x1, y1, 0.0},
                DVec3{x0, y1, 0.0}
            },
            {0.0, 0.0, -1.0}
        );
        addFan(
            mesh,
            std::array{
                DVec3{x1, y0, 0.0},
                DVec3{x1, y1, 0.0},
                DVec3{x1, y1, top},
                DVec3{x1, y0, top}
            },
            {1.0, 0.0, 0.0}
        );
        addFan(
            mesh,
            std::array{
                DVec3{x0, y0, 0.0},
                DVec3{x1, y0, 0.0},
                DVec3{x1, y0, top}
            },
            {0.0, -1.0, 0.0}
        );
        addFan(
            mesh,
            std::array{
                DVec3{x0, y1, 0.0},
                DVec3{x1, y1, 0.0},
                DVec3{x1, y1, top}
            },
            {0.0, 1.0, 0.0}
        );

        return mesh;
    }

    PaintMeshTriangles makePaintSphere(f64 radius, u32 slices, u32 stacks) {
        PaintMeshTriangles mesh;

        // rows from the north pole (+Z) down, the seam duplicated
        for(u32 stack = 0; stack <= stacks; ++stack) {
            const auto polar =
                std::numbers::pi * static_cast<f64>(stack) / stacks;
            // sin(pi) is not 0: both pole rows are snapped to their point
            const auto ring =
                stack == 0 || stack == stacks ? 0.0 : std::sin(polar);
            for(u32 slice = 0; slice <= slices; ++slice) {
                const auto azimuth =
                    2.0 * std::numbers::pi * static_cast<f64>(slice) / slices;
                const DVec3 normal{
                    ring * std::cos(azimuth),
                    ring * std::sin(azimuth),
                    std::cos(polar)
                };
                mesh.positions.push_back(toVec3(normal * radius));
                mesh.normals.push_back(toVec3(normal));
            }
        }

        const auto row = slices + 1;
        for(u32 stack = 0; stack < stacks; ++stack) {
            for(u32 slice = 0; slice < slices; ++slice) {
                const auto a = stack * row + slice;
                const auto b = a + row;
                const std::array<u32, 6> quad{a, b, a + 1, a + 1, b, b + 1};

                for(usize t = 0; t < 2; ++t) {
                    std::array<u32, 3> tri{
                        quad[3 * t],
                        quad[3 * t + 1],
                        quad[3 * t + 2]
                    };
                    const auto p0 = toDVec3(mesh.positions[tri[0]]);
                    const auto p1 = toDVec3(mesh.positions[tri[1]]);
                    const auto p2 = toDVec3(mesh.positions[tri[2]]);
                    const auto n = cross(p1 - p0, p2 - p0);
                    // a pole's triangles collapse to its point
                    if(sizeSquared(n) == 0.0)
                        continue;
                    if(dot(n, p0 + p1 + p2) < 0.0)
                        std::swap(tri[1], tri[2]);
                    mesh.indices
                        .insert(mesh.indices.end(), tri.begin(), tri.end());
                }
            }
        }

        return mesh;
    }
}
