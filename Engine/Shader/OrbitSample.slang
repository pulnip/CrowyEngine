#include "OrbitKepler.slang"

// Filling positions straight into GPU memory, so no orbit position ever crosses
// the bus.
//
// Two entry points over the same solve:
//   cs_ring    a run of consecutive samples into the trail ring, one thread per
//              (sample, body). The prefill is this with sampleCount == capacity.
//   cs_points  one position per element, for things with no history - the
//              asteroid belt, which is 2400 solves every frame on the CPU.
//
// Mirrors OrbitFillPush in Engine/RHI/Sample/Orbit/OrbitTrail.hpp.

struct OrbitFillPush{
    DescriptorHandle<StructuredBuffer<OrbitElements>> elements;  //  0
    DescriptorHandle<StructuredBuffer<OrbitPhase>> phases;       //  8
    DescriptorHandle<RWStructuredBuffer<float3>> output;         // 16
    // how many elements the tables hold, and the ring's inner stride
    uint bodyCount;                                              // 24
    // consecutive samples to write, starting at firstSlot
    uint sampleCount;                                            // 28
    uint firstSlot;                                              // 32
    // the write wraps here; cs_points ignores it
    uint capacity;                                               // 36
};

[[vk::push_constant]]
ConstantBuffer<OrbitFillPush> push : register(b0);

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_ring(uint tid: SV_DispatchThreadID){
    if(tid >= push.sampleCount * push.bodyCount)
        return;

    // sample-major, the same order the copy path writes and the vertex stage
    // reads: adjacent threads land on adjacent bodies of one instant
    const uint sampleOffset = tid / push.bodyCount;
    const uint bodyIdx = tid % push.bodyCount;
    const uint slot = (push.firstSlot + sampleOffset) % push.capacity;

    StructuredBuffer<OrbitElements> elements = push.elements;
    StructuredBuffer<OrbitPhase> phases = push.phases;
    RWStructuredBuffer<float3> output = push.output;

    output[slot * push.bodyCount + bodyIdx] = orbitKeplerPosition(
        elements[bodyIdx],
        phases[bodyIdx],
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

    // the phase epoch is the instant asked for, so there is no step to walk
    output[tid] = orbitKeplerPosition(elements[tid], phases[tid], 0);
}
