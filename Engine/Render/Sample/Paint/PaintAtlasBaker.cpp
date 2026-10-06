#include "PaintAtlasBaker.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Crowy
{
    namespace
    {
        f64 cross2(Vec2d a, Vec2d b) {
            return a.x * b.y - a.y * b.x;
        }

        Vec2d sub(Vec2d a, Vec2d b) {
            return {a.x - b.x, a.y - b.y};
        }
    }

    void PaintAtlasBaker::rasterize(
        const PaintAtlasBakeInput& in,
        std::vector<Vec4>& out
    ) {
        const auto atlasSize = in.layout.atlasSize;
        const auto count = static_cast<usize>(atlasSize) * atlasSize;
        out.assign(
            count,
            Vec4{EmptyPosition, EmptyPosition, EmptyPosition, 0.0f}
        );
        if(atlasSize <= 0)
            return;

        std::vector<f64> depth(count, std::numeric_limits<f64>::lowest());

        const auto boundsMin = in.localBounds.min;
        const auto boundsSize = in.localBounds.Size();
        const DVec3 invSize{
            boundsSize.x > SmallNumber ? 1.0 / boundsSize.x : 0.0,
            boundsSize.y > SmallNumber ? 1.0 / boundsSize.y : 0.0,
            boundsSize.z > SmallNumber ? 1.0 / boundsSize.z : 0.0
        };
        const bool hasNormals = in.normals.size() == in.positions.size();
        const auto vertexCount = static_cast<u32>(in.positions.size());

        for(const auto& island: in.layout.islands) {
            for(usize first = 0; first + 2 < in.indices.size(); first += 3) {
                const std::array<u32, 3> ids{
                    in.indices[first],
                    in.indices[first + 1],
                    in.indices[first + 2]
                };
                if(ids[0] >= vertexCount || ids[1] >= vertexCount ||
                   ids[2] >= vertexCount)
                    continue;
                const std::array<DVec3, 3> p{
                    toDVec3(in.positions[ids[0]]),
                    toDVec3(in.positions[ids[1]]),
                    toDVec3(in.positions[ids[2]])
                };

                auto faceNormal = cross(p[1] - p[0], p[2] - p[0]);
                const auto length = size(faceNormal);
                if(length <= SmallNumber)
                    continue;
                faceNormal = faceNormal / length;
                if(hasNormals) {
                    const auto vertexNormal = toDVec3(in.normals[ids[0]]) +
                                              toDVec3(in.normals[ids[1]]) +
                                              toDVec3(in.normals[ids[2]]);
                    if(dot(faceNormal, vertexNormal) < 0.0)
                        faceNormal = -faceNormal;
                }
                // a face turned from the island's viewpoint is never outermost,
                // and the reader never assigns it this island
                if(island.sign * faceNormal[island.axis] < -1e-4)
                    continue;

                std::array<DVec3, 3> normalized;
                std::array<Vec2d, 3> texel;
                std::array<f64, 3> vertexDepth;
                for(usize corner = 0; corner < 3; ++corner) {
                    normalized[corner] = (p[corner] - boundsMin) * invSize;
                    texel[corner] =
                        island.ProjectNormalized(normalized[corner]);
                    vertexDepth[corner] = island.sign * p[corner][island.axis];
                }

                const auto area2 =
                    cross2(sub(texel[1], texel[0]), sub(texel[2], texel[0]));
                if(std::abs(area2) < 1e-6)
                    continue;

                const auto minX =
                    std::min({texel[0].x, texel[1].x, texel[2].x});
                const auto maxX =
                    std::max({texel[0].x, texel[1].x, texel[2].x});
                const auto minY =
                    std::min({texel[0].y, texel[1].y, texel[2].y});
                const auto maxY =
                    std::max({texel[0].y, texel[1].y, texel[2].y});
                const auto x0 = std::max(
                    island.rect.min.x,
                    static_cast<i32>(std::floor(minX))
                );
                const auto x1 = std::min(
                    island.rect.max.x - 1,
                    static_cast<i32>(std::ceil(maxX))
                );
                const auto y0 = std::max(
                    island.rect.min.y,
                    static_cast<i32>(std::floor(minY))
                );
                const auto y1 = std::min(
                    island.rect.max.y - 1,
                    static_cast<i32>(std::ceil(maxY))
                );

                for(auto y = y0; y <= y1; ++y) {
                    for(auto x = x0; x <= x1; ++x) {
                        constexpr f64 Slack = -1e-4;
                        const Vec2d center{x + 0.5, y + 0.5};
                        const auto w0 = cross2(
                                            sub(texel[2], texel[1]),
                                            sub(center, texel[1])
                                        ) /
                                        area2;
                        const auto w1 = cross2(
                                            sub(texel[0], texel[2]),
                                            sub(center, texel[2])
                                        ) /
                                        area2;
                        const auto w2 = cross2(
                                            sub(texel[1], texel[0]),
                                            sub(center, texel[0])
                                        ) /
                                        area2;
                        if(w0 < Slack || w1 < Slack || w2 < Slack)
                            continue;

                        const auto index =
                            static_cast<usize>(y) * atlasSize + x;
                        const auto texelDepth = w0 * vertexDepth[0] +
                                                w1 * vertexDepth[1] +
                                                w2 * vertexDepth[2];
                        if(texelDepth <= depth[index])
                            continue;
                        depth[index] = texelDepth;
                        const auto position = normalized[0] * w0 +
                                              normalized[1] * w1 +
                                              normalized[2] * w2;
                        out[index] = Vec4{
                            static_cast<f32>(position.x),
                            static_cast<f32>(position.y),
                            static_cast<f32>(position.z),
                            1.0f
                        };
                    }
                }
            }
        }
    }

    void PaintAtlasBaker::computeEdgeFade(
        std::span<const Vec4> positions,
        i32 atlasSize,
        f32 fadeTexels,
        f32 seamFraction,
        std::vector<u8>& out
    ) {
        const auto count = static_cast<i32>(atlasSize * atlasSize);
        out.assign(static_cast<usize>(count), 0);
        if(count == 0)
            return;

        const auto covered = [&positions](i32 index) {
            return positions[static_cast<usize>(index)].w > 0.5f;
        };
        const auto seamSquared = seamFraction * seamFraction;
        const auto seam = [&positions, seamSquared](i32 a, i32 b) {
            const auto& pa = positions[static_cast<usize>(a)];
            const auto& pb = positions[static_cast<usize>(b)];
            const auto dx = pa.x - pb.x;
            const auto dy = pa.y - pb.y;
            const auto dz = pa.z - pb.z;

            return dx * dx + dy * dy + dz * dz > seamSquared;
        };

        constexpr auto Unreached = std::numeric_limits<i32>::max();
        std::vector<i32> distance(static_cast<usize>(count), Unreached);
        std::vector<i32> frontier;
        for(i32 y = 0; y < atlasSize; ++y) {
            for(i32 x = 0; x < atlasSize; ++x) {
                const auto index = y * atlasSize + x;
                bool edge = !covered(index) || x == 0 || y == 0 ||
                            x == atlasSize - 1 || y == atlasSize - 1;
                if(!edge) {
                    for(const auto neighbour:
                        {index - 1,
                         index + 1,
                         index - atlasSize,
                         index + atlasSize}) {
                        if(!covered(neighbour) || seam(index, neighbour)) {
                            edge = true;
                            break;
                        }
                    }
                }
                if(edge) {
                    distance[static_cast<usize>(index)] = 0;
                    frontier.push_back(index);
                }
            }
        }

        // multi-source breadth-first rings: the Chebyshev distance to the
        // nearest edge, up to the fade width
        const auto rings = std::max(1, static_cast<i32>(std::ceil(fadeTexels)));
        std::vector<i32> next;
        for(i32 ring = 1; ring <= rings && !frontier.empty(); ++ring) {
            next.clear();
            for(const auto index: frontier) {
                const auto x = index % atlasSize;
                const auto y = index / atlasSize;
                for(i32 dy = -1; dy <= 1; ++dy) {
                    for(i32 dx = -1; dx <= 1; ++dx) {
                        const auto nx = x + dx;
                        const auto ny = y + dy;
                        if(nx < 0 || ny < 0 || nx >= atlasSize ||
                           ny >= atlasSize)
                            continue;
                        const auto neighbour =
                            static_cast<usize>(ny * atlasSize + nx);
                        if(distance[neighbour] == Unreached) {
                            distance[neighbour] = ring;
                            next.push_back(static_cast<i32>(neighbour));
                        }
                    }
                }
            }
            std::swap(frontier, next);
        }

        for(i32 index = 0; index < count; ++index) {
            if(!covered(index))
                continue;
            const auto d = distance[static_cast<usize>(index)];
            const auto fade =
                d == Unreached
                    ? 1.0f
                    : std::clamp(static_cast<f32>(d) / fadeTexels, 0.0f, 1.0f);
            out[static_cast<usize>(index)] =
                static_cast<u8>(roundToInt(255.0f * fade));
        }
    }

    void PaintAtlasBaker::bake(
        const PaintAtlasBakeInput& in,
        PaintAtlasBakeOutput& out
    ) {
        std::vector<Vec4> positions;
        rasterize(in, positions);
        computeEdgeFade(
            positions,
            in.layout.atlasSize,
            in.edgeFadeTexels,
            in.edgeFadeSeamFraction,
            out.edgeFade
        );

        out.coveredTexels = 0;
        out.positions.resize(positions.size() * 4);
        for(usize i = 0; i < positions.size(); ++i) {
            const auto& p = positions[i];
            out.positions[4 * i + 0] = toHalf(p.x);
            out.positions[4 * i + 1] = toHalf(p.y);
            out.positions[4 * i + 2] = toHalf(p.z);
            out.positions[4 * i + 3] = toHalf(p.w);
            out.coveredTexels += p.w > 0.5f ? 1 : 0;
        }
    }
}
