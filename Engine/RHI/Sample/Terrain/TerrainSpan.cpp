#include <algorithm>
#include <array>
#include "TerrainDensity.hpp"
#include "TerrainSpan.hpp"

namespace{
    using namespace Crowy;

    constexpr f32 GRID_TOP = TERRAIN_CELLS_Y * TERRAIN_CELL_SIZE;

    // only called across a sign change, so the denominator cannot vanish
    constexpr f32 CrossingHeight(u32 lowerIndex, f32 lower, f32 upper) noexcept{
        const f32 t = lower / (lower - upper);

        return (lowerIndex + t) * TERRAIN_CELL_SIZE;
    }

    class MeshSink{
    private:
        std::vector<TerrainVertex>& out;
        u32 capacity;
        bool overflowed = false;

    public:
        MeshSink(std::vector<TerrainVertex>& out, u32 capacity)
            : out(out), capacity(capacity){}

        bool Overflowed() const noexcept{ return overflowed; }

        void Quad(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 normal){
            if(out.size() + 6 > capacity){
                overflowed = true;
                return;
            }

            out.push_back(TerrainVertex{.position = p0, .normal = normal});
            out.push_back(TerrainVertex{.position = p1, .normal = normal});
            out.push_back(TerrainVertex{.position = p2, .normal = normal});
            out.push_back(TerrainVertex{.position = p0, .normal = normal});
            out.push_back(TerrainVertex{.position = p2, .normal = normal});
            out.push_back(TerrainVertex{.position = p3, .normal = normal});
        }
    };

    // The parts of `span` no run in `neighbor` covers.
    // `neighbor` must be sorted and disjoint.
    template<typename Emit>
    void SubtractSpans(
        TerrainSpan span,
        std::span<const TerrainSpan> neighbor,
        Emit&& emit
    ){
        f32 cursor = span.yBottom;

        for(const auto& other: neighbor){
            if(other.yTop <= cursor)
                continue;
            if(other.yBottom >= span.yTop)
                break;

            if(other.yBottom > cursor)
                emit(cursor, other.yBottom);

            cursor = other.yTop;
            if(cursor >= span.yTop)
                return;
        }

        if(cursor < span.yTop)
            emit(cursor, span.yTop);
    }

    struct NeighborFace{
        i32 dx, dz;
        Vec3 normal;
    };

    constexpr std::array<NeighborFace, 4> NEIGHBORS{
        NeighborFace{ 1,  0, Vec3{ 1.0f, 0.0f,  0.0f}},
        NeighborFace{-1,  0, Vec3{-1.0f, 0.0f,  0.0f}},
        NeighborFace{ 0,  1, Vec3{ 0.0f, 0.0f,  1.0f}},
        NeighborFace{ 0, -1, Vec3{ 0.0f, 0.0f, -1.0f}}
    };

    void EmitWall(
        MeshSink& sink,
        const NeighborFace& face,
        f32 x0, f32 x1, f32 z0, f32 z1,
        f32 low, f32 high
    ){
        if(face.dx > 0){
            sink.Quad(
                Vec3{x1, low,  z0}, Vec3{x1, high, z0},
                Vec3{x1, high, z1}, Vec3{x1, low,  z1},
                face.normal
            );
        }
        else if(face.dx < 0){
            sink.Quad(
                Vec3{x0, low,  z0}, Vec3{x0, low,  z1},
                Vec3{x0, high, z1}, Vec3{x0, high, z0},
                face.normal
            );
        }
        else if(face.dz > 0){
            sink.Quad(
                Vec3{x0, low,  z1}, Vec3{x1, low,  z1},
                Vec3{x1, high, z1}, Vec3{x0, high, z1},
                face.normal
            );
        }
        else{
            sink.Quad(
                Vec3{x0, low,  z0}, Vec3{x0, high, z0},
                Vec3{x1, high, z0}, Vec3{x1, low,  z0},
                face.normal
            );
        }
    }
}

namespace Crowy
{
    TerrainSpanField ExtractTerrainSpans(const TerrainParams& params){
        TerrainSpanField field;
        field.columns.resize(TERRAIN_CELLS_X * TERRAIN_CELLS_Z);

        std::vector<f32> density(TERRAIN_CORNERS_Y);

        for(u32 z = 0; z < TERRAIN_CELLS_Z; ++z){
            for(u32 x = 0; x < TERRAIN_CELLS_X; ++x){
                const f32 px = (x + 0.5f) * TERRAIN_CELL_SIZE;
                const f32 pz = (z + 0.5f) * TERRAIN_CELL_SIZE;

                const f32 baseHeight = terrainBaseHeight(px, pz, params);

                for(u32 y = 0; y < TERRAIN_CORNERS_Y; ++y){
                    density[y] = terrainDensity(
                        Vec3{px, y * TERRAIN_CELL_SIZE, pz},
                        baseHeight,
                        params
                    );
                }

                auto& spans = field.columns[z * TERRAIN_CELLS_X + x];
                bool inSpan = false;
                f32 yBottom = 0.0f;

                for(u32 y = 0; y < TERRAIN_CORNERS_Y; ++y){
                    const bool solid = density[y] > 0.0f;

                    if(solid && !inSpan){
                        inSpan = true;
                        yBottom = y == 0
                            ? 0.0f
                            : CrossingHeight(y - 1, density[y - 1], density[y]);
                    }
                    else if(!solid && inSpan){
                        inSpan = false;
                        spans.push_back(TerrainSpan{
                            .yBottom = yBottom,
                            .yTop = CrossingHeight(y - 1, density[y - 1], density[y])
                        });
                    }
                }

                // run cut off by the grid ceiling
                if(inSpan)
                    spans.push_back(TerrainSpan{.yBottom = yBottom, .yTop = GRID_TOP});
            }
        }

        return field;
    }

    TerrainSpanMeshStats BuildTerrainSpanMesh(
        const TerrainSpanField& field,
        std::vector<TerrainVertex>& out,
        u32 capacityVertices,
        TerrainSpanMeshOptions options
    ){
        out.clear();
        MeshSink sink(out, capacityVertices);

        TerrainSpanMeshStats stats{
            .columnCount = static_cast<u32>(field.columns.size())
        };

        for(u32 z = 0; z < TERRAIN_CELLS_Z; ++z){
            for(u32 x = 0; x < TERRAIN_CELLS_X; ++x){
                const auto spans = field.At(x, z);

                stats.spanCount += static_cast<u32>(spans.size());
                stats.maxSpansPerColumn = std::max(
                    stats.maxSpansPerColumn,
                    static_cast<u32>(spans.size())
                );

                const f32 x0 = x * TERRAIN_CELL_SIZE;
                const f32 x1 = x0 + TERRAIN_CELL_SIZE;
                const f32 z0 = z * TERRAIN_CELL_SIZE;
                const f32 z1 = z0 + TERRAIN_CELL_SIZE;

                for(const auto& span: spans){
                    sink.Quad(
                        Vec3{x0, span.yTop, z0}, Vec3{x0, span.yTop, z1},
                        Vec3{x1, span.yTop, z1}, Vec3{x1, span.yTop, z0},
                        Vec3{0.0f, 1.0f, 0.0f}
                    );
                    sink.Quad(
                        Vec3{x0, span.yBottom, z0}, Vec3{x1, span.yBottom, z0},
                        Vec3{x1, span.yBottom, z1}, Vec3{x0, span.yBottom, z1},
                        Vec3{0.0f, -1.0f, 0.0f}
                    );

                    if(!options.emitWalls)
                        continue;

                    // a wall stands where this column is solid and the
                    // neighbour is not: the interval difference of the runs
                    for(const auto& face: NEIGHBORS){
                        SubtractSpans(
                            span,
                            field.AtOrAir(
                                static_cast<i32>(x) + face.dx,
                                static_cast<i32>(z) + face.dz
                            ),
                            [&](f32 low, f32 high){
                                EmitWall(sink, face, x0, x1, z0, z1, low, high);
                            }
                        );
                    }
                }
            }
        }

        stats.vertexCount = static_cast<u32>(out.size());
        stats.overflowed = sink.Overflowed();

        return stats;
    }
}
