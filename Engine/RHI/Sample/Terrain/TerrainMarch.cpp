#include <array>
#include <cstddef>
#include <vector>
#include "EnumUtil.hpp"
#include "MarchingCubes.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"
#include "TerrainMarch.hpp"

namespace{
    using namespace Crowy;

    constexpr CStr SHADER_PATH = "Engine/RHI/Sample/Terrain/TerrainMarch.slang";

    // must match ResourceData in Engine/RHI/Sample/Terrain/TerrainMarch.slang;
    // padding spelled out on both sides
    struct PushConstants{
        TerrainParams params;
        u32 cellsX, cellsY, cellsZ;
        f32 cellSize;
        u32 triangleCapacity;
        u32 vertexCapacity;
        u64 densityWrite;
        u64 densityRead;
        u64 vertices;
        u64 counter;
        u64 triTable;
        u64 args;
        u64 edgeVertices;
        u64 indices;
        u64 argsIndexed;
    };
    static_assert(sizeof(PushConstants) == 128);
    static_assert(offsetof(PushConstants, cellsX) == 32);
    static_assert(offsetof(PushConstants, cellSize) == 44);
    static_assert(offsetof(PushConstants, triangleCapacity) == 48);
    static_assert(offsetof(PushConstants, densityWrite) == 56);
    static_assert(offsetof(PushConstants, densityRead) == 64);
    static_assert(offsetof(PushConstants, vertices) == 72);
    static_assert(offsetof(PushConstants, counter) == 80);
    static_assert(offsetof(PushConstants, vertexCapacity) == 52);
    static_assert(offsetof(PushConstants, triTable) == 88);
    static_assert(offsetof(PushConstants, args) == 96);
    static_assert(offsetof(PushConstants, edgeVertices) == 104);
    static_assert(offsetof(PushConstants, indices) == 112);
    static_assert(offsetof(PushConstants, argsIndexed) == 120);

    // one slot per grid edge: three axes at every corner, including the
    // corners whose edge along an axis runs off the grid
    constexpr u32 EDGE_SLOT_COUNT =
        TERRAIN_CORNERS_X * TERRAIN_CORNERS_Y * TERRAIN_CORNERS_Z * 3;

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
        , argsPSO(MakePipeline(device, "cs_args"))
        , edgesPSO(MakePipeline(device, "cs_edges"))
        , indicesPSO(MakePipeline(device, "cs_indices"))
        , argsIndexedPSO(MakePipeline(device, "cs_args_indexed"))
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
            .shaderWrite = true
        }, "TerrainMarchVertices");

        counterBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(TerrainMarchCounter),
            .shaderWrite = true
        }, "TerrainMarchCounter");

        const auto triTable = MarchingCubesTriTable();
        triTableBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = static_cast<u32>(triTable.size_bytes()),
            .initialData = triTable.data()
        }, "MarchingCubesTriTable");

        argsBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(RHIDrawArgs),
            .shaderWrite = true
        }, "TerrainMarchArgs");

        edgeVertexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = EDGE_SLOT_COUNT * static_cast<u32>(sizeof(u32)),
            .shaderWrite = true
        }, "TerrainMarchEdgeVertices");

        indexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = triangleCapacity * 3 * static_cast<u32>(sizeof(u32)),
            .shaderWrite = true
        }, "TerrainMarchIndices");

        argsIndexedBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(RHIDrawIndexedArgs),
            .shaderWrite = true
        }, "TerrainMarchArgsIndexed");
    }

    TerrainMarcher::~TerrainMarcher() = default;

    TerrainMarchEdges TerrainMarcher::Record(
        RHICommandList& cmdList,
        const TerrainParams& params,
        TerrainMarchMode mode,
        TerrainMarchTargets targets
    ){
        const auto stride = static_cast<u32>(sizeof(u32));
        const bool welded = mode == TerrainMarchMode::Welded;

        const PushConstants pushConstants{
            .params = params,
            .cellsX = TERRAIN_CELLS_X,
            .cellsY = TERRAIN_CELLS_Y,
            .cellsZ = TERRAIN_CELLS_Z,
            .cellSize = TERRAIN_CELL_SIZE,
            .triangleCapacity = triangleCapacity,
            .vertexCapacity = VertexCapacity(),
            .densityWrite = densityTexture->GetWritableID(),
            .densityRead = densityTexture->GetReadableID(),
            .vertices = vertexBuffer->GetWritableID(
                static_cast<u32>(sizeof(TerrainVertex))
            ),
            .counter = counterBuffer->GetWritableID(stride),
            .triTable = triTableBuffer->GetReadableID(
                static_cast<u32>(sizeof(i32))
            ),
            .args = argsBuffer->GetWritableID(stride),
            .edgeVertices = edgeVertexBuffer->GetWritableID(stride),
            .indices = indexBuffer->GetWritableID(stride),
            .argsIndexed = argsIndexedBuffer->GetWritableID(stride)
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

        auto& modeArgs = welded ? *argsIndexedBuffer : *argsBuffer;

        const TerrainMarchEdges edges{
            .vertices = MakeBarrier(*vertexBuffer,
                RHIResourceUsage::StorageCompute,
                targets.vertices
            ),
            .counter = MakeBarrier(*counterBuffer,
                RHIResourceUsage::StorageCompute,
                targets.counter
            ),
            .args = MakeBarrier(modeArgs,
                RHIResourceUsage::StorageCompute,
                targets.args
            ),
            .indices = MakeBarrier(*indexBuffer,
                RHIResourceUsage::StorageCompute,
                targets.indices
            )
        };

        {
            const std::array textureAcquires{densityEdge};

            std::vector<RHIBufferBarrier> bufferAcquires{
                AcquireForWrite(*vertexBuffer, vertexResting),
                AcquireForWrite(*counterBuffer, counterResting),
                AcquireForWrite(modeArgs, welded ? argsIndexedResting : argsResting)
            };
            if(welded){
                bufferAcquires.push_back(
                    AcquireForWrite(*indexBuffer, indexResting)
                );
                bufferAcquires.push_back(
                    AcquireForWrite(*edgeVertexBuffer, edgeVertexResting)
                );
            }
            cmdList.BeginComputePass(textureAcquires, bufferAcquires);

            cmdList.SetPipelineState(*clearPSO);
            cmdList.SetPushComputeConstants(pushConstants);
            cmdList.Dispatch({1, 1, 1});

            // whatever runs next accumulates into the counter the clear just
            // zeroed: a UAV -> UAV hazard inside one pass
            const std::array counterHazard{
                MakeBarrier(*counterBuffer,
                    RHIResourceUsage::StorageCompute,
                    RHIResourceUsage::StorageCompute
                )
            };
            cmdList.DispatchBarrier({}, counterHazard);

            if(welded){
                cmdList.SetPipelineState(*edgesPSO);
                cmdList.SetPushComputeConstants(pushConstants);
                cmdList.Dispatch({
                    TERRAIN_CORNERS_X, TERRAIN_CORNERS_Y, TERRAIN_CORNERS_Z
                });

                // the index pass reads the slots the vertex pass just filled,
                // and the counter it advanced
                const std::array weldHazard{
                    counterHazard[0],
                    MakeBarrier(*edgeVertexBuffer,
                        RHIResourceUsage::StorageCompute,
                        RHIResourceUsage::StorageCompute
                    )
                };
                cmdList.DispatchBarrier({}, weldHazard);

                cmdList.SetPipelineState(*indicesPSO);
            }
            else{
                cmdList.SetPipelineState(*marchPSO);
            }
            cmdList.SetPushComputeConstants(pushConstants);
            cmdList.Dispatch({
                TERRAIN_CELLS_X, TERRAIN_CELLS_Y, TERRAIN_CELLS_Z
            });

            // the argument pass reads the count that just accumulated
            cmdList.DispatchBarrier({}, counterHazard);

            cmdList.SetPipelineState(welded ? *argsIndexedPSO : *argsPSO);
            cmdList.SetPushComputeConstants(pushConstants);
            cmdList.Dispatch({1, 1, 1});

            std::vector<RHIBufferBarrier> releases{
                edges.vertices, edges.counter, edges.args
            };
            if(welded)
                releases.push_back(edges.indices);

            cmdList.EndComputePass({}, releases);
        }

        densityResting = RHIResourceUsage::SampledCompute;
        vertexResting = targets.vertices;
        counterResting = targets.counter;
        if(welded){
            argsIndexedResting = targets.args;
            indexResting = targets.indices;
            edgeVertexResting = RHIResourceUsage::StorageCompute;
        }
        else{
            argsResting = targets.args;
        }

        return edges;
    }
}
