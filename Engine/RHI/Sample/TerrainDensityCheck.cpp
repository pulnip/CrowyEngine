#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <print>
#include <span>
#include <vector>
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "Terrain.hpp"
#include "TerrainDensity.hpp"

// Holds the CPU density field (TerrainDensity.hpp) against the GPU one
// (TerrainDensity.slang) over the same points, so a change made to one side
// alone fails the smoke run. Also checks the field is not degenerate: two
// implementations that both return zero agree perfectly and are useless.

namespace{
    using namespace Crowy;

    // lattice offsets keep samples inside noise cells and away from floor() ties,
    // where the two sides could disagree for irrelevant reasons
    constexpr u32 LATTICE_X = 17;
    constexpr u32 LATTICE_Y = 9;
    constexpr u32 LATTICE_Z = 17;
    constexpr u32 POINT_COUNT = LATTICE_X * LATTICE_Y * LATTICE_Z;

    // absolute floor + relative term for the compilers' float reassociation;
    // actual drift is far larger than this
    constexpr f32 Tolerance(f32 reference) noexcept{
        return 1e-3f + 1e-4f * std::abs(reference);
    }

    struct PushConstants{
        TerrainParams params;
        u32 pointCount;
        u32 _pad0 = 0;
        u64 points;
        u64 results;
    };
    // must match ResourceData in Engine/Shader/TerrainDensityCheck.slang;
    // padding spelled out on both sides
    static_assert(sizeof(PushConstants) == 56);
    static_assert(offsetof(PushConstants, pointCount) == 32);
    static_assert(offsetof(PushConstants, points) == 40);
    static_assert(offsetof(PushConstants, results) == 48);

    struct DensitySample{
        f32 baseHeight;
        f32 density;
    };
    static_assert(sizeof(DensitySample) == 8);

    std::vector<Vec4> MakeSamplePoints(){
        std::vector<Vec4> points;
        points.reserve(POINT_COUNT);

        constexpr f32 STEP_X = static_cast<f32>(TERRAIN_CELLS_X) / (LATTICE_X - 1);
        constexpr f32 STEP_Y = static_cast<f32>(TERRAIN_CELLS_Y) / (LATTICE_Y - 1);
        constexpr f32 STEP_Z = static_cast<f32>(TERRAIN_CELLS_Z) / (LATTICE_Z - 1);

        for(u32 z = 0; z < LATTICE_Z; ++z){
            for(u32 y = 0; y < LATTICE_Y; ++y){
                for(u32 x = 0; x < LATTICE_X; ++x){
                    points.push_back(Vec4{
                        .x = x * STEP_X + 0.37f,
                        .y = y * STEP_Y + 0.13f,
                        .z = z * STEP_Z + 0.71f
                    });
                }
            }
        }

        return points;
    }

    bool CheckFieldIsAlive(
        std::span<const Vec4> points,
        std::span<const DensitySample> samples
    ){
        f32 minHeight = std::numeric_limits<f32>::max();
        f32 maxHeight = std::numeric_limits<f32>::lowest();
        bool anySolid = false, anyAir = false;
        u32 carvedCount = 0;

        for(usize i = 0; i < points.size(); ++i){
            minHeight = std::min(minHeight, samples[i].baseHeight);
            maxHeight = std::max(maxHeight, samples[i].baseHeight);

            anySolid |= samples[i].density > 0.0f;
            anyAir |= samples[i].density < 0.0f;

            // below the surface but not solid: only the cave carve does that
            if(points[i].y < samples[i].baseHeight && samples[i].density < 0.0f)
                ++carvedCount;
        }

        std::println("  baseHeight range: [{:.3f}, {:.3f}]", minHeight, maxHeight);
        std::println("  carved samples:   {} / {}", carvedCount, points.size());

        bool alive = true;
        if(maxHeight - minHeight < 1.0f){
            std::println("  FAIL: the surface is flat - is fbm2D returning a constant?");
            alive = false;
        }
        if(!anySolid || !anyAir){
            std::println("  FAIL: density never changes sign - there is no surface to extract");
            alive = false;
        }
        if(carvedCount == 0){
            std::println("  FAIL: nothing was carved - is the cave noise reaching the threshold?");
            alive = false;
        }

        return alive;
    }

    bool CompareAgainstCPU(
        std::span<const Vec4> points,
        std::span<const DensitySample> gpu,
        const TerrainParams& params
    ){
        f32 worstHeightDiff = 0.0f, worstDensityDiff = 0.0f;
        usize worstIndex = 0;
        u32 mismatches = 0;

        for(usize i = 0; i < points.size(); ++i){
            const auto& p = points[i];

            const f32 baseHeight = terrainBaseHeight(p.x, p.z, params);
            const f32 density = terrainDensity(static_cast<Vec3>(p), baseHeight, params);

            const f32 heightDiff = std::abs(baseHeight - gpu[i].baseHeight);
            const f32 densityDiff = std::abs(density - gpu[i].density);

            if(heightDiff > worstHeightDiff){
                worstHeightDiff = heightDiff;
                worstIndex = i;
            }
            worstDensityDiff = std::max(worstDensityDiff, densityDiff);

            if(heightDiff > Tolerance(baseHeight) || densityDiff > Tolerance(density)){
                if(mismatches < 5){
                    std::println(
                        "  mismatch at ({:.3f}, {:.3f}, {:.3f}): "
                        "baseHeight cpu {:.6f} gpu {:.6f}, density cpu {:.6f} gpu {:.6f}",
                        p.x, p.y, p.z,
                        baseHeight, gpu[i].baseHeight,
                        density, gpu[i].density
                    );
                }
                ++mismatches;
            }
        }

        std::println("  worst baseHeight delta: {:.3e}", worstHeightDiff);
        std::println("  worst density delta:    {:.3e}", worstDensityDiff);

        if(mismatches > 0){
            std::println(
                "  FAIL: {} of {} points disagree - "
                "the CPU and GPU density fields have drifted apart",
                mismatches, points.size()
            );
            std::println("  worst point index: {}", worstIndex);

            return false;
        }

        return true;
    }
}

int main(void){
    try{
        using namespace Crowy;

        const TerrainParams params{};
        const auto points = MakeSamplePoints();

        auto device = CreateDevice();
        auto cmdList = device->CreateCommandList();

        const auto pointBytes = static_cast<u32>(sizeof(Vec4) * points.size());
        const auto resultBytes = static_cast<u32>(sizeof(DensitySample) * points.size());

        auto pointBuffer = device->CreateBuffer(RHIBufferCreateDesc{
            .size = pointBytes,
            .usage = RHIBufferUsage::ShaderResource,
            .access = RHIMemoryAccess::GPUOnly,
            .initialData = points.data()
        }, "TerrainSamplePoints");
        auto resultBuffer = device->CreateBuffer(RHIBufferCreateDesc{
            .size = resultBytes,
            .usage = RHIBufferUsage::UnorderedAccess,
            .access = RHIMemoryAccess::GPUOnly
        }, "TerrainDensityResults");
        auto readback = device->CreateBuffer(RHIBufferCreateDesc{
            .size = resultBytes,
            .usage = RHIBufferUsage::CopyDst,
            .access = RHIMemoryAccess::CPURead
        }, "TerrainDensityReadback");

        auto pipeline = device->CreatePipelineState(RHIComputePipelineStateDesc{
            .computeShader = {
                .path = "Engine/Shader/TerrainDensityCheck.slang",
                .entryPoint = "cs_main"
            }
        });

        cmdList->Begin();

        // the compute pass releases the results, the copy pass acquires
        // them - one edge, same value on both ends
        const auto resultEdge = MakeBarrier(*resultBuffer,
            RHIResourceUsage::StorageCompute,
            RHIResourceUsage::CopySrc
        );

        {
            const std::array acquires{
                MakeBarrier(*resultBuffer,
                    RHIResourceUsage::Undefined,
                    RHIResourceUsage::StorageCompute
                )
            };
            cmdList->BeginComputePass({}, acquires);

            cmdList->SetPipelineState(*pipeline);
            cmdList->SetPushComputeConstants(PushConstants{
                .params = params,
                .pointCount = static_cast<u32>(points.size()),
                .points = pointBuffer->GetReadableID(
                    static_cast<u32>(sizeof(Vec4))
                ),
                .results = resultBuffer->GetWritableID(
                    static_cast<u32>(sizeof(DensitySample))
                )
            });
            cmdList->Dispatch({static_cast<u32>(points.size()), 1, 1});

            const std::array releases{resultEdge};
            cmdList->EndComputePass({}, releases);
        }

        {
            const std::array acquires{resultEdge};
            cmdList->BeginBlitPass({}, acquires);
            cmdList->Copy(*resultBuffer, *readback, 0, 0, resultBytes);
            cmdList->EndBlitPass();
        }

        cmdList->Close();
        RHICommandList* cmdLists[] = {cmdList.get()};
        device->Submit(cmdLists);

        device->WaitFrame(1);

        // forced push for resolve the in-flight state
        device->GetFrameIndexRef() += RHI_FRAMES_IN_FLIGHT - 1;

        std::vector<DensitySample> gpu(points.size());
        readback->Download(gpu.data(), resultBytes);

        std::println("TerrainDensityCheck: {} points", points.size());

        const bool alive = CheckFieldIsAlive(points, gpu);
        const bool matches = CompareAgainstCPU(points, gpu, params);

        if(!alive || !matches)
            return 1;

        std::println("Succeed!");
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
