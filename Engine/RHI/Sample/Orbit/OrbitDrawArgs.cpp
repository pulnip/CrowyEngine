#include <array>
#include "EnumUtil.hpp"
#include "OrbitDrawArgs.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace{
    using namespace Crowy;

    constexpr u32 BODY_STRIDE = static_cast<u32>(sizeof(OrbitBodyDraw));
    // RHIDrawArgs as four uints; the shader writes it that way
    constexpr u32 ARG_STRIDE = static_cast<u32>(sizeof(u32));
    constexpr u32 ARGS_PER_DRAW = 4;
    static_assert(sizeof(RHIDrawArgs) == ARGS_PER_DRAW * sizeof(u32));

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
}

namespace Crowy
{
    OrbitTrailArgs::OrbitTrailArgs(
        RHIDevice& device,
        std::span<const OrbitBodyDraw> table,
        RHIResourceUsage argsNextUse
    )
        : pso(device.CreatePipelineState(RHIComputePipelineStateDesc{
            .computeShader = {
                .path = "Engine/Shader/OrbitSample.slang",
                .entryPoint = "cs_trail_args"
            }
        }))
        , count(static_cast<u32>(table.size()))
        , argsNextUse(argsNextUse)
    {
        CROWY_ASSERT(count > 0);
        CROWY_ASSERT(count <= 32,
            "the enabled mask is one bit per body"
        );

        // Read-only from here on, which is what lets it carry initial contents
        // at all: the creation upload is an access, and a resource that has
        // been accessed can never be acquired from Undefined again.
        bodies = device.CreateBuffer(RHIBufferCreateDesc{
            .size = count * BODY_STRIDE,
            .usage = RHIBufferUsage::ShaderResource,
            .location = RHIMemoryLocation::Device,
            .initialData = table.data()
        }, "OrbitBodyDraws");

        segCounts = device.CreateBuffer(RHIBufferCreateDesc{
            .size = count * static_cast<u32>(sizeof(u32)),
            .usage = combine(
                RHIBufferUsage::ShaderResource,
                RHIBufferUsage::UnorderedAccess
            ),
            .location = RHIMemoryLocation::Device
        }, "OrbitSegCounts");

        args = device.CreateBuffer(RHIBufferCreateDesc{
            .size = count * static_cast<u32>(sizeof(RHIDrawArgs)),
            .usage = combine(
                RHIBufferUsage::IndirectArgument,
                RHIBufferUsage::UnorderedAccess,
                // the headless check reads them back
                RHIBufferUsage::CopySrc
            ),
            .location = RHIMemoryLocation::Device
        }, "OrbitTrailArgs");
    }

    OrbitTrailArgs::~OrbitTrailArgs() = default;

    OrbitTrailArgs::Edges OrbitTrailArgs::Record(
        RHICommandList& cmdList,
        u32 filled,
        f32 orbitTurns,
        f32 dayPerSample,
        u32 enabledMask
    ){
        CROWY_ASSERT(dayPerSample > 0.0f);

        const Edges edges{
            .segCounts = MakeBarrier(*segCounts,
                RHIResourceUsage::StorageCompute,
                RHIResourceUsage::SampledVertex
            ),
            .args = MakeBarrier(*args,
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
        cmdList.SetPushComputeConstants(OrbitArgsPush{
            .bodies = bodies->GetReadableID(BODY_STRIDE),
            .args = args->GetWritableID(ARG_STRIDE),
            .segCounts = segCounts->GetWritableID(ARG_STRIDE),
            .bodyCount = count,
            .filled = filled,
            .enabledMask = enabledMask,
            .orbitTurns = orbitTurns,
            .dayPerSample = dayPerSample
        });
        cmdList.Dispatch({count, 1, 1});

        cmdList.EndComputePass({}, releases);

        segCountsResting = RHIResourceUsage::SampledVertex;
        argsResting = argsNextUse;

        return edges;
    }
}
