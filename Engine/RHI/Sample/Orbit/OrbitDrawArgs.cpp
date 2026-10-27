#include "OrbitDrawArgs.hpp"

#include <array>

#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace
{
    using namespace Crowy;

    constexpr u32 BODY_STRIDE = static_cast<u32>(sizeof(OrbitBodyDraw));
    // the shader writes RHIDrawArgs as four uints
    constexpr u32 ARG_STRIDE = static_cast<u32>(sizeof(u32));
    constexpr u32 ARGS_PER_DRAW = 4;
    static_assert(sizeof(RHIDrawArgs) == ARGS_PER_DRAW * sizeof(u32));

    // Cross-submission, except on the first Record, which has no release to
    // pair with.
    RHIBufferBarrier AcquireForWrite(
        RHIBuffer& buffer,
        RHIResourceUsage resting
    ) {
        return resting == RHIResourceUsage::Undefined
                   ? makeBarrier(
                         buffer,
                         RHIResourceUsage::Undefined,
                         RHIResourceUsage::StorageCompute
                     )
                   : makeCrossSubmissionBarrier(
                         buffer,
                         resting,
                         RHIResourceUsage::StorageCompute
                     );
    }
}

namespace Crowy
{
    OrbitTrailArgs::OrbitTrailArgs(
        RHIDevice& device,
        std::span<const OrbitBodyDraw> table,
        RHIResourceUsage argsNextUse
    )
        : pso(device.CreatePipelineState(
              RHIComputePipelineStateDesc{
                  .computeShader =
                      {.path = "Engine/RHI/Sample/Orbit/OrbitArgs.slang",
                       .entryPoint = "cs_trail_args"}
              }
          )),
          count(static_cast<u32>(table.size())),
          argsNextUse(argsNextUse) {
        CROWY_ASSERT(count > 0);
        CROWY_ASSERT(count <= 32, "the enabled mask is one bit per body");

        // The creation upload is an access, and an accessed resource can never
        // be acquired from Undefined again, so only this table carries one.
        bodies = device.CreateBuffer(
            RHIBufferCreateDesc{
                .size = count * BODY_STRIDE,
                .initialData = table.data()
            },
            "OrbitBodyDraws"
        );

        segCounts = device.CreateBuffer(
            RHIBufferCreateDesc{
                .size = count * static_cast<u32>(sizeof(u32)),
                .shaderWrite = true
            },
            "OrbitSegCounts"
        );

        args = device.CreateBuffer(
            RHIBufferCreateDesc{
                .size = count * static_cast<u32>(sizeof(RHIDrawArgs)),
                .shaderWrite = true
            },
            "OrbitTrailArgs"
        );
    }

    OrbitTrailArgs::~OrbitTrailArgs() = default;

    OrbitTrailArgs::Edges OrbitTrailArgs::Record(
        RHICommandList& cmdList,
        u32 filled,
        f32 orbitTurns,
        f32 dayPerSample,
        u32 enabledMask
    ) {
        CROWY_ASSERT(dayPerSample > 0.0f);

        const Edges edges{
            .segCounts = makeBarrier(
                *segCounts,
                RHIResourceUsage::StorageCompute,
                RHIResourceUsage::SampledVertex
            ),
            .args = makeBarrier(
                *args,
                RHIResourceUsage::StorageCompute,
                argsNextUse
            )
        };

        const std::array acquires{
            AcquireForWrite(*segCounts, segCountsResting),
            AcquireForWrite(*args, argsResting)
        };
        const std::array releases{edges.segCounts, edges.args};

        cmdList.BeginComputePass({}, acquires);

        cmdList.SetPipelineState(*pso);
        cmdList.SetPushComputeConstants(
            OrbitArgsPush{
                .bodies = bodies->GetReadableID(BODY_STRIDE),
                .args = args->GetWritableID(ARG_STRIDE),
                .segCounts = segCounts->GetWritableID(ARG_STRIDE),
                .bodyCount = count,
                .filled = filled,
                .enabledMask = enabledMask,
                .orbitTurns = orbitTurns,
                .dayPerSample = dayPerSample
            }
        );
        cmdList.Dispatch({count, 1, 1});

        cmdList.EndComputePass({}, releases);

        segCountsResting = RHIResourceUsage::SampledVertex;
        argsResting = argsNextUse;

        return edges;
    }
}
