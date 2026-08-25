#include <algorithm>
#include <array>
#include <cmath>
#include <print>
#include <span>
#include <vector>
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "Terrain.hpp"
#include "TerrainDensity.hpp"
#include "TerrainMarch.hpp"

// Runs the density and marching passes once and reads the result back, so the
// smoke run fails if the GPU mesher stops landing on the isosurface.
//
// A marching cubes vertex always sits on a cell edge, which is what makes it
// checkable without a CPU mesher: the edge can be recovered from the position
// alone, and the CPU density at its two corners must straddle zero and read
// zero again at the point the GPU picked.

namespace{
    using namespace Crowy;

    // measured at the default parameters; see the count this prints
    constexpr u32 TRIANGLE_CAPACITY = 1'500'000;
    // how much comes back. The atomic hands out slots in no particular order,
    // so a prefix is an unbiased sample of the whole mesh. Welding needs the
    // vertex prefix to cover every vertex an index can name, which it does by
    // a wide margin - welded vertices number about half the triangles
    constexpr u32 PREFIX_VERTICES = 300'000;
    constexpr u32 PREFIX_INDICES = 300'000;

    // the CPU and GPU density fields agree to about this much
    // (TerrainDensityCheck measures it); the interpolated residual inherits
    // that error without amplifying it
    constexpr f32 DENSITY_TOLERANCE = 1e-2f;
    // a coordinate is on a grid line if it is this close to one
    constexpr f32 LATTICE_TOLERANCE = 1e-3f;
    // below this a triangle is too thin for its winding to mean anything
    constexpr f32 MIN_TRIANGLE_AREA = 0.01f * TERRAIN_CELL_SIZE * TERRAIN_CELL_SIZE;

    constexpr f32 GRID_MAX_X = TERRAIN_CELLS_X * TERRAIN_CELL_SIZE;
    constexpr f32 GRID_MAX_Y = TERRAIN_CELLS_Y * TERRAIN_CELL_SIZE;
    constexpr f32 GRID_MAX_Z = TERRAIN_CELLS_Z * TERRAIN_CELL_SIZE;

    struct Failures{
        u32 outOfBounds = 0;
        u32 offLattice = 0;
        u32 edgeDoesNotCross = 0;
        u32 offSurface = 0;
        u32 badNormal = 0;

        u32 Total() const noexcept{
            return outOfBounds + offLattice + edgeDoesNotCross +
                offSurface + badNormal;
        }
    };

    // Whether the triangles are wound the way the engine expects.
    //
    // Per-triangle this is an ill-conditioned question: a sliver's face normal
    // is mostly rounding error, and a small triangle across a sharp fold can
    // genuinely disagree with the smooth gradient normals at its corners. A
    // reversed winding, though, is not a local accident - it would flip the
    // whole mesh at once. So the verdict rides on the area-weighted total,
    // which slivers barely move, and the per-triangle rate is reported for
    // diagnosis rather than judged.
    struct Winding{
        u32 tested = 0;
        u32 disagreed = 0;
        // sum of cross(...) . normal, so each triangle counts for its area
        f64 weighted = 0.0;
        // the disagreeing triangle furthest from agreeing
        f32 worstCosine = 1.0f;
        f32 worstArea = 0.0f;
        Vec3 worstAt{};
    };

    f32 Axis(Vec3 v, u32 axis) noexcept{
        return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
    }

    void SetAxis(Vec3& v, u32 axis, f32 value) noexcept{
        (axis == 0 ? v.x : (axis == 1 ? v.y : v.z)) = value;
    }

    bool InsideGrid(Vec3 p) noexcept{
        return p.x >= -LATTICE_TOLERANCE && p.x <= GRID_MAX_X + LATTICE_TOLERANCE
            && p.y >= -LATTICE_TOLERANCE && p.y <= GRID_MAX_Y + LATTICE_TOLERANCE
            && p.z >= -LATTICE_TOLERANCE && p.z <= GRID_MAX_Z + LATTICE_TOLERANCE;
    }

    // the axis the vertex was interpolated along, or 3 if it landed on a
    // corner (t was 0 or 1, which the noise makes vanishingly rare) - and 4
    // if two axes are off the lattice, which no edge vertex can be
    u32 InterpolatedAxis(Vec3 p) noexcept{
        u32 found = 3, count = 0;

        for(u32 axis = 0; axis < 3; ++axis){
            const f32 grid = Axis(p, axis) / TERRAIN_CELL_SIZE;
            if(std::abs(grid - std::round(grid)) > LATTICE_TOLERANCE){
                found = axis;
                ++count;
            }
        }

        return count > 1 ? 4 : found;
    }

    // The residual of the linear interpolation the shader did, evaluated with
    // CPU densities. Conditioning does not enter: this asks whether the
    // reported point reads zero, not where exactly zero was.
    bool OnSurface(Vec3 p, u32 axis, const TerrainParams& params, Failures& failures){
        const f32 coord = Axis(p, axis);
        const f32 low = std::floor(coord / TERRAIN_CELL_SIZE) * TERRAIN_CELL_SIZE;

        Vec3 a = p, b = p;
        SetAxis(a, axis, low);
        SetAxis(b, axis, low + TERRAIN_CELL_SIZE);

        const f32 da = terrainDensity(a, params);
        const f32 db = terrainDensity(b, params);

        if((da > 0.0f) == (db > 0.0f)){
            ++failures.edgeDoesNotCross;
            return false;
        }

        const f32 t = (coord - low) / TERRAIN_CELL_SIZE;
        if(std::abs(da + t * (db - da)) > DENSITY_TOLERANCE){
            ++failures.offSurface;
            return false;
        }

        return true;
    }

    Failures CheckVertices(
        std::span<const TerrainVertex> vertices,
        const TerrainParams& params
    ){
        Failures failures;

        for(const auto& vertex: vertices){
            if(!InsideGrid(vertex.position)){
                ++failures.outOfBounds;
                continue;
            }

            if(std::abs(norm(vertex.normal) - 1.0f) > 1e-2f)
                ++failures.badNormal;

            const u32 axis = InterpolatedAxis(vertex.position);
            if(axis == 4){
                ++failures.offLattice;
                continue;
            }
            // landed on a corner: there is no edge to reconstruct
            if(axis == 3)
                continue;

            OnSurface(vertex.position, axis, params, failures);
        }

        return failures;
    }

    // the engine rasterizes the right-hand normal as the front face
    // (left-handed, frontCounterClockwise = false), so the winding has to
    // agree with the gradient normals the shader stored
    Winding CheckWinding(std::span<const TerrainVertex> vertices){
        Winding winding;

        for(usize i = 0; i + 2 < vertices.size(); i += 3){
            const auto& a = vertices[i];
            const auto& b = vertices[i + 1];
            const auto& c = vertices[i + 2];

            const auto face = cross(b.position - a.position, c.position - a.position);
            const auto smooth = a.normal + b.normal + c.normal;

            winding.weighted += dot(face, smooth);

            const f32 area = 0.5f * norm(face);
            if(area < MIN_TRIANGLE_AREA)
                continue;

            ++winding.tested;

            const f32 cosine = dot(face, smooth) / (norm(face) * norm(smooth));
            if(cosine > 0.0f)
                continue;

            ++winding.disagreed;
            if(cosine < winding.worstCosine){
                winding.worstCosine = cosine;
                winding.worstArea = area;
                winding.worstAt = a.position;
            }
        }

        return winding;
    }

    // Expands the welded mesh back into a triangle soup so the same geometry
    // checks apply, and judges the indices on the way through: every one has
    // to name a vertex that was actually written.
    bool BuildWeldedSoup(
        std::span<const u32> indices,
        std::span<const TerrainVertex> stored,
        const TerrainMarchCounter& counter,
        std::vector<TerrainVertex>& soup
    ){
        const u32 triangles = counter.DrawableTriangles();

        std::println("  {} vertices welded, {:.2f} per triangle",
            counter.vertexCount,
            static_cast<f32>(counter.vertexCount) / triangles
        );

        if(counter.vertexCount > PREFIX_VERTICES){
            std::println(
                "  FAIL: {} vertices is more than the {} read back, so the "
                "indices cannot be resolved - raise PREFIX_VERTICES",
                counter.vertexCount, PREFIX_VERTICES
            );
            return false;
        }
        // welding that saves nothing is welding that did not happen
        if(counter.vertexCount >= triangles * 3){
            std::println(
                "  FAIL: {} vertices for {} triangles - no sharing at all, "
                "so the edge slots are not being reused",
                counter.vertexCount, triangles
            );
            return false;
        }

        const auto count = std::min(triangles * 3, PREFIX_INDICES);
        soup.reserve(count);

        for(u32 i = 0; i + 2 < count; i += 3){
            for(u32 k = 0; k < 3; ++k){
                const u32 index = indices[i + k];
                if(index >= counter.vertexCount){
                    std::println(
                        "  FAIL: index {} at slot {} names vertex {} of {} - "
                        "the edge slots and the vertex counter disagree",
                        index, i + k, index, counter.vertexCount
                    );
                    return false;
                }
                soup.push_back(stored[index]);
            }
        }

        return true;
    }

    // The draw arguments the argument kernel wrote. Nothing else ever computes
    // this on the CPU, so getting it wrong would only show as a mis-sized draw.
    bool ArgsAreRight(
        RHIBuffer& readback,
        const TerrainMarchCounter& counter,
        TerrainMarchMode mode
    ){
        const u32 expected = counter.DrawableTriangles() * 3;

        // the two layouts agree on the first three fields and differ after,
        // so only the tail needs telling apart
        RHIDrawArgs args{};
        u32 baseVertex = 0;
        if(mode == TerrainMarchMode::Welded){
            RHIDrawIndexedArgs indexed{};
            readback.Download(&indexed, sizeof(indexed));

            args = RHIDrawArgs{
                .vertexCount = indexed.indexCount,
                .instanceCount = indexed.instanceCount,
                .firstVertex = indexed.firstIndex,
                .baseInstance = indexed.baseInstance
            };
            baseVertex = static_cast<u32>(indexed.baseVertex);
        }
        else{
            readback.Download(&args, sizeof(args));
        }

        if(baseVertex != 0){
            std::println("  FAIL: baseVertex should be 0, not {}", baseVertex);
            return false;
        }

        const auto unit = mode == TerrainMarchMode::Welded ? "indices" : "vertices";
        std::println("  draw args: {} {}, {} instances",
            args.vertexCount, unit, args.instanceCount
        );

        if(args.vertexCount != expected){
            std::println(
                "  FAIL: the argument kernel asked for {} {} where the counter "
                "says {}",
                args.vertexCount, unit, expected
            );
            return false;
        }
        if(args.instanceCount != 1 || args.firstVertex != 0 || args.baseInstance != 0){
            std::println(
                "  FAIL: draw args carry junk beyond the vertex count "
                "({} instances, first {}, base {})",
                args.instanceCount, args.firstVertex, args.baseInstance
            );
            return false;
        }

        return true;
    }

    // a reversed winding turns the weighted total negative and sends the rate
    // towards everything, so this sits orders of magnitude away from both
    constexpr f32 MAX_DISAGREEMENT_RATE = 0.005f;

    bool WindingIsRight(const Winding& winding){
        if(winding.tested == 0)
            return false;

        const f32 rate = static_cast<f32>(winding.disagreed) / winding.tested;

        std::println(
            "  winding: {} of {} triangles disagree ({:.4f}%), "
            "area-weighted total {:+.3e}",
            winding.disagreed, winding.tested, 100.0f * rate, winding.weighted
        );
        if(winding.disagreed > 0){
            std::println(
                "    worst: cos {:+.3f}, area {:.5f}, at ({:.2f}, {:.2f}, {:.2f})",
                winding.worstCosine, winding.worstArea,
                winding.worstAt.x, winding.worstAt.y, winding.worstAt.z
            );
        }

        if(winding.weighted <= 0.0){
            std::println(
                "  FAIL: the mesh faces inward - "
                "reverse the emit order in cs_march"
            );
            return false;
        }
        if(rate > MAX_DISAGREEMENT_RATE){
            std::println(
                "  FAIL: too many triangles wound against their normals for "
                "slivers to explain"
            );
            return false;
        }

        return true;
    }

    void Report(const Failures& failures, u32 checked){
        std::println("  vertices checked: {}", checked);

        if(failures.Total() == 0){
            std::println("  every vertex sits on a cell edge at density zero");
            return;
        }

        if(failures.outOfBounds > 0){
            std::println("  FAIL: {} vertices outside the grid", failures.outOfBounds);
        }
        if(failures.offLattice > 0){
            std::println(
                "  FAIL: {} vertices off the cell edges - "
                "the edge table and the corner offsets disagree",
                failures.offLattice
            );
        }
        if(failures.edgeDoesNotCross > 0){
            std::println(
                "  FAIL: {} vertices on edges whose corners are both solid or "
                "both air - the case index does not match the table",
                failures.edgeDoesNotCross
            );
        }
        if(failures.offSurface > 0){
            std::println(
                "  FAIL: {} vertices away from density zero - "
                "the edge interpolation is wrong",
                failures.offSurface
            );
        }
        if(failures.badNormal > 0){
            std::println("  FAIL: {} normals are not unit length", failures.badNormal);
        }
    }
}

namespace{
    // Runs one mode end to end and judges what came back. Both modes mesh the
    // same field, so the same geometry checks apply to both - the welded one
    // just has to be dereferenced through its indices first.
    bool CheckMode(
        RHIDevice& device,
        const TerrainParams& params,
        TerrainMarchMode mode
    ){
        const bool welded = mode == TerrainMarchMode::Welded;
        std::println("--- {} ---", welded ? "welded" : "soup");

        auto warmupList = device.CreateCommandList();
        auto cmdList = device.CreateCommandList();

        TerrainMarcher marcher(device, TRIANGLE_CAPACITY);

        const auto vertexBytes =
            PREFIX_VERTICES * static_cast<u32>(sizeof(TerrainVertex));
        const auto indexBytes = PREFIX_INDICES * static_cast<u32>(sizeof(u32));
        const auto argsBytes = static_cast<u32>(
            welded ? sizeof(RHIDrawIndexedArgs) : sizeof(RHIDrawArgs)
        );

        auto counterReadback = device.CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(TerrainMarchCounter),
            .memory = RHIMemoryType::CPURead
        }, "TerrainMarchCounterReadback");
        auto vertexReadback = device.CreateBuffer(RHIBufferCreateDesc{
            .size = vertexBytes,
            .memory = RHIMemoryType::CPURead
        }, "TerrainMarchVertexReadback");
        auto indexReadback = device.CreateBuffer(RHIBufferCreateDesc{
            .size = indexBytes,
            .memory = RHIMemoryType::CPURead
        }, "TerrainMarchIndexReadback");
        auto argsReadback = device.CreateBuffer(RHIBufferCreateDesc{
            .size = argsBytes,
            .memory = RHIMemoryType::CPURead
        }, "TerrainMarchArgsReadback");

        // A first run, submitted on its own, left the way a frame that drew
        // the result would leave it. The run that follows then has to wind
        // every buffer back from a read to a write across a submission
        // boundary - the hazard auto-rebuild hits on every parameter change,
        // and the reason this runs twice instead of once. The debug layer is
        // what actually judges it.
        {
            warmupList->Begin();
            marcher.Record(*warmupList, params, mode);
            warmupList->Close();

            RHICommandList* warmup[] = {warmupList.get()};
            device.Submit(warmup);
            device.WaitFrame(device.GetSubmittedFrame());
        }

        cmdList->Begin();

        const auto edges = marcher.Record(*cmdList, params, mode, {
            .vertices = RHIResourceUsage::CopySrc,
            .counter = RHIResourceUsage::CopySrc,
            .args = RHIResourceUsage::CopySrc,
            .indices = RHIResourceUsage::CopySrc
        });

        {
            std::vector acquires{edges.vertices, edges.counter, edges.args};
            if(welded)
                acquires.push_back(edges.indices);

            cmdList->BeginBlitPass({}, acquires);
            cmdList->Copy(
                marcher.Counter(), *counterReadback,
                0, 0, sizeof(TerrainMarchCounter)
            );
            cmdList->Copy(marcher.Vertices(), *vertexReadback, 0, 0, vertexBytes);
            cmdList->Copy(marcher.Args(mode), *argsReadback, 0, 0, argsBytes);
            if(welded)
                cmdList->Copy(marcher.Indices(), *indexReadback, 0, 0, indexBytes);

            cmdList->EndBlitPass();
        }

        cmdList->Close();
        RHICommandList* cmdLists[] = {cmdList.get()};
        device.Submit(cmdLists);

        device.WaitFrame(device.GetSubmittedFrame());
        TerrainMarchCounter counter;
        counterReadback->Download(&counter, sizeof(counter));

        std::println("  {} triangles of {} capacity ({:.1f}%)",
            counter.triangleCount,
            TRIANGLE_CAPACITY,
            100.0f * counter.triangleCount / TRIANGLE_CAPACITY
        );

        if(counter.overflowed != 0){
            std::println(
                "  FAIL: capacity ran out after {} triangles - "
                "raise TRIANGLE_CAPACITY past {}",
                counter.firstDropped, counter.triangleCount
            );
            return false;
        }
        if(counter.triangleCount == 0){
            std::println(
                "  FAIL: no triangles - the case index never leaves 0 or 255, "
                "so the density texture is probably empty"
            );
            return false;
        }

        std::vector<TerrainVertex> stored(PREFIX_VERTICES);
        vertexReadback->Download(stored.data(), vertexBytes);

        std::vector<TerrainVertex> soup;
        bool indicesAreRight = true;

        if(welded){
            std::vector<u32> indices(PREFIX_INDICES);
            indexReadback->Download(indices.data(), indexBytes);

            indicesAreRight = BuildWeldedSoup(indices, stored, counter, soup);
        }
        else{
            // whole triangles only, and never past what was actually written
            const auto emitted = std::min(
                counter.DrawableTriangles() * 3,
                PREFIX_VERTICES
            );
            stored.resize(emitted - emitted % 3);
            soup = std::move(stored);
        }

        const auto failures = CheckVertices(soup, params);
        Report(failures, static_cast<u32>(soup.size()));

        const bool wound = WindingIsRight(CheckWinding(soup));
        const bool drawable = ArgsAreRight(*argsReadback, counter, mode);

        return failures.Total() == 0 && wound && drawable && indicesAreRight;
    }
}

int main(void){
    try{
        using namespace Crowy;

        const TerrainParams params{};

        auto device = CreateDevice();

        std::println("TerrainMarchCheck");

        // both run even if the first fails, so one report covers both
        bool ok = CheckMode(*device, params, TerrainMarchMode::Soup);
        ok = CheckMode(*device, params, TerrainMarchMode::Welded) && ok;

        if(!ok)
            return 1;

        std::println("Succeed!");
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
