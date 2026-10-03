#include "IslandMeshes.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "Island/IslandScene.h"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"

namespace
{
    using namespace Crowy;

    u32 addVertex(MeshData& mesh, Vec3 position, Vec3 normal, Vec2 texCoord) {
        // around y for the sides, along x for the caps
        const auto tangent = std::abs(normal.y) > 0.9f
                                 ? unitX()
                                 : normalize(cross(unitY(), normal));
        mesh.vertices.push_back(
            Vertex{
                .position = position,
                .normal = normal,
                .texCoord = texCoord,
                .tangent = toVec4(tangent, 1.0f)
            }
        );

        return static_cast<u32>(mesh.vertices.size() - 1);
    }

    // a triangle wound so it fronts `outward`
    void addTriangle(MeshData& mesh, u32 a, u32 b, u32 c, Vec3 outward) {
        const auto& pa = mesh.vertices[a].position;
        const auto& pb = mesh.vertices[b].position;
        const auto& pc = mesh.vertices[c].position;
        if(dot(cross(pb - pa, pc - pa), outward) < 0.0f)
            std::swap(b, c);
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    }

    // a flat quad over four corners in order around it
    void addQuad(
        MeshData& mesh,
        const std::array<Vec3, 4>& corners,
        Vec3 outward
    ) {
        constexpr std::array<Vec2, 4> TexCoords{
            Vec2{0.0f, 1.0f},
            Vec2{1.0f, 1.0f},
            Vec2{1.0f, 0.0f},
            Vec2{0.0f, 0.0f}
        };

        std::array<u32, 4> ids{};
        for(usize i = 0; i < 4; ++i)
            ids[i] = addVertex(mesh, corners[i], outward, TexCoords[i]);
        addTriangle(mesh, ids[0], ids[1], ids[2], outward);
        addTriangle(mesh, ids[0], ids[2], ids[3], outward);
    }
}

namespace Crowy
{
    MeshData makeEllipsoid(Vec3 radii) {
        auto mesh = MakeSphere(1.0f, 48, 24);
        for(auto& vertex: mesh.vertices) {
            const auto unit = vertex.position;
            const auto normal = normalize(
                Vec3{unit.x / radii.x, unit.y / radii.y, unit.z / radii.z}
            );
            const auto tangent = static_cast<Vec3>(vertex.tangent) * radii;
            vertex.position = unit * radii;
            vertex.normal = normal;
            vertex.tangent = toVec4(
                normalize(tangent - normal * dot(normal, tangent)),
                vertex.tangent.w
            );
        }

        return mesh;
    }

    MeshData makeCylinder(f32 radius, f32 halfLength, u32 segments) {
        constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;

        MeshData mesh;
        const auto around = [&](u32 i) {
            const auto angle = TwoPi * static_cast<f32>(i) /
                               static_cast<f32>(segments);
            return Vec3{std::sin(angle), 0.0f, std::cos(angle)};
        };
        for(u32 i = 0; i < segments; ++i) {
            const auto from = around(i);
            const auto to = around(i + 1);
            const auto outward = normalize(from + to);
            addQuad(
                mesh,
                {from * radius - halfLength * unitY(),
                 to * radius - halfLength * unitY(),
                 to * radius + halfLength * unitY(),
                 from * radius + halfLength * unitY()},
                outward
            );
        }
        for(const auto side: {-1.0f, 1.0f}) {
            const auto up = side * unitY();
            const auto center =
                addVertex(mesh, up * halfLength, up, Vec2{0.5f, 0.5f});
            for(u32 i = 0; i < segments; ++i) {
                const auto a = addVertex(
                    mesh,
                    around(i) * radius + up * halfLength,
                    up,
                    Vec2{0.0f, 0.0f}
                );
                const auto b = addVertex(
                    mesh,
                    around(i + 1) * radius + up * halfLength,
                    up,
                    Vec2{1.0f, 0.0f}
                );
                addTriangle(mesh, center, a, b, up);
            }
        }

        return mesh;
    }

    MeshData makeTipiCanvas() {
        constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;
        constexpr auto FacetAngle =
            TwoPi / static_cast<f32>(ISLAND_TIPI_FACETS);
        // how far up the poles, from the base to their crossing, the canvas
        // reaches
        constexpr auto Reach = (ISLAND_TIPI_CANVAS_TOP_Y - ISLAND_TIPI_BASE_Y) /
                               (ISLAND_TIPI_APEX_Y - ISLAND_TIPI_BASE_Y);
        constexpr auto Middle =
            0.5f * (ISLAND_TIPI_BASE_Y + ISLAND_TIPI_CANVAS_TOP_Y);

        const Vec3 apex{0.0f, ISLAND_TIPI_APEX_Y - Middle, 0.0f};
        const auto pole = [&](u32 k) {
            const auto angle =
                ISLAND_TIPI_FIRST_POLE + FacetAngle * static_cast<f32>(k);
            return Vec3{
                ISLAND_TIPI_RADIUS * std::sin(angle),
                ISLAND_TIPI_BASE_Y - Middle,
                ISLAND_TIPI_RADIUS * std::cos(angle)
            };
        };

        MeshData mesh;
        for(u32 k = ISLAND_TIPI_OPEN_FACETS; k < ISLAND_TIPI_FACETS; ++k) {
            const auto from = pole(k);
            const auto to = pole(k + 1);
            const auto fromTop = from + (apex - from) * Reach;
            const auto toTop = to + (apex - to) * Reach;
            auto outward = normalize(cross(to - from, fromTop - from));
            if(dot(outward, Vec3{from.x + to.x, 0.0f, from.z + to.z}) < 0.0f)
                outward = -outward;
            addQuad(mesh, {from, to, toTop, fromTop}, outward);
        }

        return mesh;
    }
}
