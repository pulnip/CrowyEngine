#include <algorithm>
#include <array>
#include <cmath>
#include <print>
#include <span>
#include <vector>
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
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
    // how much of the soup comes back. The atomic hands out slots in no
    // particular order, so a prefix is an unbiased sample of the whole mesh
    constexpr u32 PREFIX_VERTICES = 300'000;

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

    // The draw arguments cs_args wrote. Nothing else ever computes this on
    // the CPU, so getting it wrong would only show as a mis-sized draw.
    bool ArgsAreRight(const RHIDrawArgs& args, const TerrainMarchCounter& counter){
        const u32 expected = counter.DrawableTriangles() * 3;

        std::println("  draw args: {} vertices, {} instances",
            args.vertexCount, args.instanceCount
        );

        if(args.vertexCount != expected){
            std::println(
                "  FAIL: cs_args asked for {} vertices where the counter says {}",
                args.vertexCount, expected
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

int main(void){
    try{
        using namespace Crowy;

        const TerrainParams params{};

        auto device = CreateDevice();
        auto warmupList = device->CreateCommandList();
        auto cmdList = device->CreateCommandList();
        auto fence = device->CreateFence();

        TerrainMarcher marcher(*device, TRIANGLE_CAPACITY);

        const auto prefixBytes =
            PREFIX_VERTICES * static_cast<u32>(sizeof(TerrainVertex));

        auto counterReadback = device->CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(TerrainMarchCounter),
            .usage = RHIBufferUsage::CopyDst,
            .access = RHIMemoryAccess::CPURead
        }, "TerrainMarchCounterReadback");
        auto vertexReadback = device->CreateBuffer(RHIBufferCreateDesc{
            .size = prefixBytes,
            .usage = RHIBufferUsage::CopyDst,
            .access = RHIMemoryAccess::CPURead
        }, "TerrainMarchVertexReadback");
        auto argsReadback = device->CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(RHIDrawArgs),
            .usage = RHIBufferUsage::CopyDst,
            .access = RHIMemoryAccess::CPURead
        }, "TerrainMarchArgsReadback");

        // A first run, submitted on its own, left the way a frame that drew
        // the result would leave it. The run that follows then has to wind
        // every buffer back from a read to a write across a submission
        // boundary - the hazard auto-rebuild hits on every parameter change,
        // and the reason this sample runs twice instead of once. The debug
        // layer is what actually judges it.
        {
            warmupList->Begin();
            marcher.Record(*warmupList, params);
            warmupList->Close();

            RHICommandList* warmup[] = {warmupList.get()};
            device->Submit(warmup, *fence);
            fence->WaitCPU(1);
        }

        cmdList->Begin();

        const auto edges = marcher.Record(*cmdList, params, {
            .vertices = RHIResourceUsage::CopySrc,
            .counter = RHIResourceUsage::CopySrc,
            .args = RHIResourceUsage::CopySrc
        });

        {
            const std::array acquires{edges.vertices, edges.counter, edges.args};
            cmdList->BeginBlitPass({}, acquires);
            cmdList->Copy(
                marcher.Counter(), *counterReadback,
                0, 0, sizeof(TerrainMarchCounter)
            );
            cmdList->Copy(
                marcher.Vertices(), *vertexReadback,
                0, 0, prefixBytes
            );
            cmdList->Copy(
                marcher.Args(), *argsReadback,
                0, 0, sizeof(RHIDrawArgs)
            );
            cmdList->EndBlitPass();
        }

        cmdList->Close();
        RHICommandList* cmdLists[] = {cmdList.get()};
        device->Submit(cmdLists, *fence);

        fence->WaitCPU(2);

        // forced push for resolve the in-flight state
        device->GetFrameIndexRef() += RHI_FRAMES_IN_FLIGHT - 1;

        TerrainMarchCounter counter;
        counterReadback->Download(&counter, sizeof(counter));

        std::println("TerrainMarchCheck: {} triangles of {} capacity ({:.1f}%)",
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
            return 1;
        }
        if(counter.triangleCount == 0){
            std::println(
                "  FAIL: no triangles - the case index never leaves 0 or 255, "
                "so the density texture is probably empty"
            );
            return 1;
        }

        std::vector<TerrainVertex> vertices(PREFIX_VERTICES);
        vertexReadback->Download(vertices.data(), prefixBytes);

        // whole triangles only, and never past what was actually written
        const auto emitted = std::min(
            counter.DrawableTriangles() * 3,
            PREFIX_VERTICES
        );
        vertices.resize(emitted - emitted % 3);

        const auto failures = CheckVertices(vertices, params);
        Report(failures, static_cast<u32>(vertices.size()));

        const bool wound = WindingIsRight(CheckWinding(vertices));

        RHIDrawArgs args{};
        argsReadback->Download(&args, sizeof(args));
        const bool drawable = ArgsAreRight(args, counter);

        if(failures.Total() > 0 || !wound || !drawable)
            return 1;

        std::println("Succeed!");
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
