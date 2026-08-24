#include "OrbitFill.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace{
    using namespace Crowy;

    constexpr CStr SHADER_PATH = "Engine/Shader/OrbitSample.slang";

    RHIComputePipelineStateRAII MakePipeline(RHIDevice& device, CStr entryPoint){
        return device.CreatePipelineState(RHIComputePipelineStateDesc{
            .computeShader = {.path = SHADER_PATH, .entryPoint = entryPoint}
        });
    }

    constexpr u32 ELEMENT_STRIDE =
        static_cast<u32>(sizeof(OrbitElementsGPU));
    constexpr u32 PHASE_STRIDE =
        static_cast<u32>(sizeof(OrbitPhaseGPU));
    constexpr u32 POSITION_STRIDE =
        static_cast<u32>(sizeof(Vec3));
}

namespace Crowy
{
    OrbitKeplerFill::OrbitKeplerFill(
        RHIDevice& device,
        std::span<const OrbitalElements> table
    )
        : ringPSO(MakePipeline(device, "cs_ring"))
        , pointPSO(MakePipeline(device, "cs_points"))
        , device(device)
        , elements(table.begin(), table.end())
        , phaseScratch(table.size())
    {
        CROWY_ASSERT(!elements.empty());

        std::vector<OrbitElementsGPU> packed;
        packed.reserve(elements.size());
        for(const auto& el: elements){
            packed.push_back(MakeElementsGPU(el));
        }

        elementBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = static_cast<u32>(packed.size() * ELEMENT_STRIDE),
            .usage = RHIBufferUsage::ShaderResource,
            .location = RHIMemoryLocation::Device,
            .initialData = packed.data()
        }, "OrbitElements");

    }

    OrbitKeplerFill::~OrbitKeplerFill() = default;

    void OrbitKeplerFill::SetEpoch(f64 day, f64 dayPerSample){
        for(usize i=0; i<elements.size(); ++i){
            phaseScratch[i] = MakePhaseGPU(elements[i], day, dayPerSample);
        }
    }

    // The slice waits for the dispatch rather than riding SetEpoch: a
    // transient range is only good until the batch it was recorded into
    // completes, so it has to be taken by whoever records that batch.
    void OrbitKeplerFill::uploadPhases(){
        phaseSlice = device.UploadTransient(
            std::span<const OrbitPhaseGPU>(phaseScratch),
            PHASE_STRIDE
        );
    }

    void OrbitKeplerFill::RecordRing(
        RHICommandList& cmdList,
        RHIBuffer& output,
        u32 firstSlot,
        u32 sampleCount,
        u32 capacity
    ){
        CROWY_ASSERT(sampleCount > 0 && sampleCount <= capacity);
        CROWY_ASSERT(firstSlot < capacity);
        CROWY_ASSERT(sampleCount <= ORBIT_PHASE_MAX_STEPS,
            "the fixed-point phase splits over two levels of "
            "ORBIT_PHASE_BLOCK; a longer run would start accumulating"
        );

        const auto count = Count();

        uploadPhases();

        cmdList.SetPipelineState(*ringPSO);
        cmdList.SetPushComputeConstants(OrbitFillPush{
            .elements = elementBuffer->GetReadableID(ELEMENT_STRIDE),
            .phases = phaseSlice.buffer->GetReadableID(PHASE_STRIDE),
            .output = output.GetWritableID(POSITION_STRIDE),
            .bodyCount = count,
            .sampleCount = sampleCount,
            .firstSlot = firstSlot,
            .capacity = capacity,
            .phaseBase = phaseSlice.offset / PHASE_STRIDE
        });
        cmdList.Dispatch({sampleCount * count, 1, 1});
    }

    void OrbitKeplerFill::RecordPoints(
        RHICommandList& cmdList,
        RHIBuffer& output
    ){
        const auto count = Count();

        uploadPhases();

        cmdList.SetPipelineState(*pointPSO);
        cmdList.SetPushComputeConstants(OrbitFillPush{
            .elements = elementBuffer->GetReadableID(ELEMENT_STRIDE),
            .phases = phaseSlice.buffer->GetReadableID(PHASE_STRIDE),
            .output = output.GetWritableID(POSITION_STRIDE),
            .bodyCount = count,
            .sampleCount = 1,
            .firstSlot = 0,
            .capacity = 1,
            .phaseBase = phaseSlice.offset / PHASE_STRIDE
        });
        cmdList.Dispatch({count, 1, 1});
    }
}
