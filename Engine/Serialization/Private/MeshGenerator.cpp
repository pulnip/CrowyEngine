#include <array>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"

namespace Crowy
{
    MeshData MakeBox(f32 halfSize){
        struct Face{
            Vec3 normal;
            // direction of increasing u.
            Vec3 tangent;
        };
        constexpr std::array faces = {
            Face{.normal = { 1.0f,  0.0f,  0.0f}, .tangent = { 0.0f, 0.0f,  1.0f}},
            Face{.normal = {-1.0f,  0.0f,  0.0f}, .tangent = { 0.0f, 0.0f, -1.0f}},
            Face{.normal = { 0.0f,  1.0f,  0.0f}, .tangent = { 1.0f, 0.0f,  0.0f}},
            Face{.normal = { 0.0f, -1.0f,  0.0f}, .tangent = { 1.0f, 0.0f,  0.0f}},
            Face{.normal = { 0.0f,  0.0f,  1.0f}, .tangent = {-1.0f, 0.0f,  0.0f}},
            Face{.normal = { 0.0f,  0.0f, -1.0f}, .tangent = { 1.0f, 0.0f,  0.0f}}
        };
        constexpr std::array<Vec2, 4> texCoords = {
            Vec2{0.0f, 0.0f},
            Vec2{1.0f, 0.0f},
            Vec2{1.0f, 1.0f},
            Vec2{0.0f, 1.0f}
        };

        std::vector<Vertex> vertices;
        vertices.reserve(faces.size() * texCoords.size());

        std::vector<u32> indices;
        indices.reserve(faces.size() * 6);

        for(const auto& [normal, tangent]: faces){
            // the corners are laid out along this, so it is the direction of
            // increasing v by construction, hence the constant handedness of 1
            const auto bitangent = cross(normal, tangent);
            const auto base = static_cast<u32>(vertices.size());

            for(const auto& uv: texCoords){
                const f32 su = 2.0f * (uv.x - 0.5f) * halfSize;
                const f32 sv = 2.0f * (uv.y - 0.5f) * halfSize;

                vertices.push_back(Vertex{
                    .position = halfSize*normal + su*tangent + sv*bitangent,
                    .normal = normal,
                    .texCoord = uv,
                    .tangent = Vec4{tangent.x, tangent.y, tangent.z, 1.0f}
                });
            }

            indices.insert(indices.end(), {
                base + 0, base + 1, base + 2,
                base + 0, base + 2, base + 3
            });
        }

        return MeshData{
            .vertices = std::move(vertices),
            .indices = std::move(indices)
        };
    }

    MeshData MakePlane(Vec3 normal, Vec3 tangent, f32 halfSize, f32 uvScale){
        // the corners are laid out along this, so it is the direction of
        // increasing v, exactly as in MakeBox
        const auto bitangent = cross(normal, tangent);

        constexpr std::array<Vec2, 4> corners = {
            Vec2{0.0f, 0.0f},
            Vec2{1.0f, 0.0f},
            Vec2{1.0f, 1.0f},
            Vec2{0.0f, 1.0f}
        };

        std::vector<Vertex> vertices;
        vertices.reserve(corners.size());

        for(const auto& corner: corners){
            const f32 su = 2.0f * (corner.x - 0.5f) * halfSize;
            const f32 sv = 2.0f * (corner.y - 0.5f) * halfSize;

            vertices.push_back(Vertex{
                .position = su*tangent + sv*bitangent,
                .normal = normal,
                .texCoord = uvScale * corner,
                .tangent = Vec4{tangent.x, tangent.y, tangent.z, 1.0f}
            });
        }

        return MeshData{
            .vertices = std::move(vertices),
            .indices = {0, 1, 2, 0, 2, 3}
        };
    }

    MeshData MakePlane(f32 halfSize, f32 uvScale){
        return MakePlane(
            Vec3{0.0f, 1.0f, 0.0f},
            Vec3{1.0f, 0.0f, 0.0f},
            halfSize, uvScale
        );
    }

    MeshData MakeSphere(f32 radius, u32 slices, u32 stacks){
        // below this the sphere has no surface left: every ring would be a pole
        CROWY_ASSERT(slices >= 3, "MakeSphere needs at least 3 slices");
        CROWY_ASSERT(stacks >= 2, "MakeSphere needs at least 2 stacks");

        constexpr f32 pi = std::numbers::pi_v<f32>;
        const f32 dTheta = 2.0f * pi / slices;
        const f32 dPhi = pi / stacks;

        std::vector<Vertex> vertices;
        vertices.reserve((slices + 1) * (stacks + 1));

        for(u32 i=0; i<=stacks; ++i){
            // phi grows from the north pole downward, so v does too
            const f32 phi = dPhi * i;
            const f32 cosp = std::cos(phi), sinp = std::sin(phi);
            const f32 v = static_cast<f32>(i) / stacks;

            for(u32 j=0; j<=slices; ++j){
                const f32 theta = dTheta * j;
                const f32 cost = std::cos(theta), sint = std::sin(theta);
                const f32 u = static_cast<f32>(j) / slices;

                const Vec3 normal{sinp * cost, cosp, sinp * sint};

                vertices.push_back(Vertex{
                    .position = radius * normal,
                    .normal = normal,
                    .texCoord = Vec2{u, v},
                    // d(position)/d(theta) normalized, which stays unit length
                    // even at the poles where the position itself degenerates.
                    // cross(normal, tangent) is d(position)/d(phi), so the
                    // handedness is 1 here as well
                    .tangent = Vec4{-sint, 0.0f, cost, 1.0f}
                });
            }
        }

        std::vector<u32> indices;
        // every ring of quads makes 2 triangles per quad,
        // except the two pole rings, where one of the two collapses
        indices.reserve(3 * slices * (2*stacks - 2));

        for(u32 i=0; i<stacks; ++i){
            const u32 base = (slices + 1) * i;

            for(u32 j=0; j<slices; ++j){
                const u32 topLeft = base + j;
                const u32 topRight = base + (j + 1);
                const u32 bottomLeft = base + (slices + 1) + j;
                const u32 bottomRight = base + (slices + 1) + (j + 1);

                // the top row of the first ring sits entirely on the north pole,
                // so topLeft and topRight coincide and this triangle has no area.
                // the pole vertices still differ in u and tangent, which is why
                // they are kept as separate vertices
                if(i != 0){
                    indices.insert(indices.end(), {
                        topLeft, topRight, bottomRight
                    });
                }
                // and symmetrically, the last ring's bottom row is the south pole
                if(i != stacks - 1){
                    indices.insert(indices.end(), {
                        topLeft, bottomRight, bottomLeft
                    });
                }
            }
        }

        return MeshData{
            .vertices = std::move(vertices),
            .indices = std::move(indices)
        };
    }
}
