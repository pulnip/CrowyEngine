#include "OrbitCommon.slang"
#include "OrbitKepler.slang"

// Mirrors OrbitFillPush in OrbitFill.hpp.
struct OrbitFillPush{
    DescriptorHandle<StructuredBuffer<OrbitElements>> elements;  //  0
    DescriptorHandle<StructuredBuffer<OrbitPhase>> phases;       //  8
    DescriptorHandle<RWStructuredBuffer<float3>> output;         // 16
    uint bodyCount;                                              // 24
    uint sampleCount;                                            // 28
    uint firstSlot;                                              // 32
    uint capacity;                                               // 36
    uint phaseBase;                                              // 40
    uint _pad0;                                                  // 44
};

[[vk::push_constant]]
ConstantBuffer<OrbitFillPush> push : register(b0);

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_ring(uint tid: SV_DispatchThreadID){
    if(tid >= push.sampleCount * push.bodyCount)
        return;

    // sample-major, the order the copy path writes and the vertex stage reads
    const uint sampleOffset = tid / push.bodyCount;
    const uint bodyIdx = tid % push.bodyCount;
    const uint slot = (push.firstSlot + sampleOffset) % push.capacity;

    StructuredBuffer<OrbitElements> elements = push.elements;
    StructuredBuffer<OrbitPhase> phases = push.phases;
    RWStructuredBuffer<float3> output = push.output;

    output[slot * push.bodyCount + bodyIdx] = orbitKeplerPosition(
        elements[bodyIdx],
        phases[push.phaseBase + bodyIdx],
        sampleOffset
    );
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_points(uint tid: SV_DispatchThreadID){
    if(tid >= push.bodyCount)
        return;

    StructuredBuffer<OrbitElements> elements = push.elements;
    StructuredBuffer<OrbitPhase> phases = push.phases;
    RWStructuredBuffer<float3> output = push.output;

    output[tid] = orbitKeplerPosition(
        elements[tid], phases[push.phaseBase + tid], 0
    );
}
