#include <array>
#include <cstddef>
#include "EnumUtil.hpp"
#include "MarchingCubes.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"
#include "TerrainMarch.hpp"

namespace{
    using namespace Crowy;

    constexpr CStr SHADER_PATH = "Engine/Shader/TerrainMarch.slang";

    // must match ResourceData in Engine/Shader/TerrainMarch.slang;
    // padding spelled out on both sides
    struct PushConstants{
        TerrainParams params;
        u32 cellsX, cellsY, cellsZ;
        f32 cellSize;
        u32 triangleCapacity;
        u32 _pad0 = 0;
        u64 densityWrite;
        u64 densityRead;
        u64 vertices;
        u64 counter;
        u64 triTable;
    };
    static_assert(sizeof(PushConstants) == 96);
    static_assert(offsetof(PushConstants, cellsX) == 32);
    static_assert(offsetof(PushConstants, cellSize) == 44);
    static_assert(offsetof(PushConstants, triangleCapacity) == 48);
    static_assert(offsetof(PushConstants, densityWrite) == 56);
    static_assert(offsetof(PushConstants, densityRead) == 64);
    static_assert(offsetof(PushConstants, vertices) == 72);
    static_assert(offsetof(PushConstants, counter) == 80);
    static_assert(offsetof(PushConstants, triTable) == 88);

    RHIComputePipelineStateRAII MakePipeline(RHIDevice& device, CStr entryPoint){
        return device.CreatePipelineState(RHIComputePipelineStateDesc{
            .computeShader = {.path = SHADER_PATH, .entryPoint = entryPoint}
        });
    }

    // The first Record has nothing to pair with; later ones wind the resource
    // back from wherever the previous submission left it.
    RHIBufferBarrier AcquireForWrite(RHIBuffer& buffer, RHIResourceUsage resting){
        return resting == RHIResourceUsage::Undefined ?
            MakeBarrier(buffer,
                RHIResourceUsage::Undefined,
                RHIResourceUsage::StorageCompute
            ) :
            MakeCrossSubmissionBarrier(buffer,
                resting,
                RHIResourceUsage::StorageCompute
            );
    }

    RHITextureBarrier AcquireForWrite(RHITexture& texture, RHIResourceUsage resting){
        // every texel is rewritten, so the old contents can go
        return resting == RHIResourceUsage::Undefined ?
            MakeBarrier(texture,
                RHIResourceUsage::Undefined,
                RHIResourceUsage::StorageCompute
            ) :
            MakeCrossSubmissionBarrier(texture,
                resting,
                RHIResourceUsage::StorageCompute,
                /*discardContents=*/true
            );
    }
}

namespace Crowy
{
    TerrainMarcher::TerrainMarcher(RHIDevice& device, u32 triangleCapacity)
        : triangleCapacity(triangleCapacity)
        , clearPSO(MakePipeline(device, "cs_clear"))
        , densityPSO(MakePipeline(device, "cs_density"))
        , marchPSO(MakePipeline(device, "cs_march"))
    {
        densityTexture = device.CreateTexture(RHITextureCreateDesc{
            .width = TERRAIN_CORNERS_X,
            .height = TERRAIN_CORNERS_Y,
            .depth = TERRAIN_CORNERS_Z,
            .format = RHIPixelFormat::R32_FLOAT,
            .usage = combine(
                RHITextureUsage::ShaderResource,
                RHITextureUsage::UnorderedAccess
            )
        }, "TerrainDensity");

        vertexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = VertexCapacity() * static_cast<u32>(sizeof(TerrainVertex)),
            .usage = combine(
                RHIBufferUsage::ShaderResource,
                RHIBufferUsage::UnorderedAccess,
                RHIBufferUsage::CopySrc
            ),
            .access = RHIMemoryAccess::GPUOnly
        }, "TerrainMarchVertices");

        counterBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(TerrainMarchCounter),
            .usage = combine(
                RHIBufferUsage::UnorderedAccess,
                RHIBufferUsage::CopySrc
            ),
            .access = RHIMemoryAccess::GPUOnly
        }, "TerrainMarchCounter");

        const auto triTable = MarchingCubesTriTable();
        triTableBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = static_cast<u32>(triTable.size_bytes()),
            .usage = RHIBufferUsage::ShaderResource,
            .access = RHIMemoryAccess::GPUOnly,
            .initialData = triTable.data()
        }, "MarchingCubesTriTable");
    }

    TerrainMarcher::~TerrainMarcher() = default;

    TerrainMarcher::Edges TerrainMarcher::Record(
        RHICommandList& cmdList,
        const TerrainParams& params,
        RHIResourceUsage verticesAfter,
        RHIResourceUsage counterAfter
    ){
        const PushConstants pushConstants{
            .params = params,
            .cellsX = TERRAIN_CELLS_X,
            .cellsY = TERRAIN_CELLS_Y,
            .cellsZ = TERRAIN_CELLS_Z,
            .cellSize = TERRAIN_CELL_SIZE,
            .triangleCapacity = triangleCapacity,
            .densityWrite = densityTexture->GetWritableID(),
            .densityRead = densityTexture->GetReadableID(),
            .vertices = vertexBuffer->GetWritableID(
                static_cast<u32>(sizeof(TerrainVertex))
            ),
            .counter = counterBuffer->GetWritableID(
                static_cast<u32>(sizeof(u32))
            ),
            .triTable = triTableBuffer->GetReadableID(
                static_cast<u32>(sizeof(i32))
            )
        };

        // the density pass releases the field, the marching pass acquires it -
        // one edge, same value on both ends
        const auto densityEdge = MakeBarrier(*densityTexture,
            RHIResourceUsage::StorageCompute,
            RHIResourceUsage::SampledCompute
        );

        {
            const std::array acquires{
                AcquireForWrite(*densityTexture, densityResting)
            };
            cmdList.BeginComputePass(acquires);

            cmdList.SetPipelineState(*densityPSO);
            cmdList.SetPushComputeConstants(pushConstants);
            cmdList.Dispatch({
                TERRAIN_CORNERS_X, TERRAIN_CORNERS_Y, TERRAIN_CORNERS_Z
            });

            const std::array releases{densityEdge};
            cmdList.EndComputePass(releases);
        }

        const Edges edges{
            .vertices = MakeBarrier(*vertexBuffer,
                RHIResourceUsage::StorageCompute,
                verticesAfter
            ),
            .counter = MakeBarrier(*counterBuffer,
                RHIResourceUsage::StorageCompute,
                counterAfter
            )
        };

        {
            const std::array textureAcquires{densityEdge};
            const std::array bufferAcquires{
                AcquireForWrite(*vertexBuffer, vertexResting),
                AcquireForWrite(*counterBuffer, counterResting)
            };
            cmdList.BeginComputePass(textureAcquires, bufferAcquires);

            cmdList.SetPipelineState(*clearPSO);
            cmdList.SetPushComputeConstants(pushConstants);
            cmdList.Dispatch({1, 1, 1});

            // the marching pass accumulates into the counter the clear just
            // zeroed: a UAV -> UAV hazard inside one pass
            const std::array hazards{
                MakeBarrier(*counterBuffer,
                    RHIResourceUsage::StorageCompute,
                    RHIResourceUsage::StorageCompute
                )
            };
            cmdList.DispatchBarrier({}, hazards);

            cmdList.SetPipelineState(*marchPSO);
            cmdList.SetPushComputeConstants(pushConstants);
            cmdList.Dispatch({
                TERRAIN_CELLS_X, TERRAIN_CELLS_Y, TERRAIN_CELLS_Z
            });

            const std::array releases{edges.vertices, edges.counter};
            cmdList.EndComputePass({}, releases);
        }

        densityResting = RHIResourceUsage::SampledCompute;
        vertexResting = verticesAfter;
        counterResting = counterAfter;

        return edges;
    }
}
