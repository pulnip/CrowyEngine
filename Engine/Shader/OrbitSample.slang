#include "OrbitCommon.slang"
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
    // row 0 of this dispatch's phase slice inside the shared buffer
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

    // the phase epoch is the instant asked for, so there is no step to walk
    output[tid] = orbitKeplerPosition(
        elements[tid], phases[push.phaseBase + tid], 0
    );
}

// ---------------------------------------------------------------------------
// Indirect draw arguments for the trails.
//
// Nine draws, one per body, issued by a single ExecuteIndirect. What the CPU
// used to work out per frame - how many segments each body draws, given how
// full the ring is and where the panel's controls sit - is a division per body,
// so this exists less to save that work than to close the loop: the counts and
// the segCount the vertex stage fades against are now written by the same pass,
// and cannot drift.
//
// DrawBatch::countBuffer is reserved, so the draw count stays nine. A body the
// panel switched off gets instanceCount 0 and costs an empty draw.

struct OrbitArgsPush{
    // read only: colour and period never change
    DescriptorHandle<StructuredBuffer<BodyDraw>> bodies;    //  0
    // RHIDrawArgs[bodyCount], four uints each
    DescriptorHandle<RWStructuredBuffer<uint>> args;        //  8
    // one segment count per body, the same number the arguments carry
    DescriptorHandle<RWStructuredBuffer<uint>> segCounts;   // 16
    uint bodyCount;                                         // 24
    // samples the ring actually holds
    uint filled;                                            // 28
    // bit b set means body b is drawn
    uint enabledMask;                                       // 32
    float orbitTurns;                                       // 36
    float dayPerSample;                                     // 40
    uint _pad0;                                             // 44
};

[[vk::push_constant]]
ConstantBuffer<OrbitArgsPush> argsPush : register(b0);

// four vertices per instance, a triangle strip expanded in the vertex shader
static const uint TRAIL_VERTEX_COUNT = 4;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_trail_args(uint tid: SV_DispatchThreadID){
    if(tid >= argsPush.bodyCount)
        return;

    StructuredBuffer<BodyDraw> bodies = argsPush.bodies;
    RWStructuredBuffer<uint> segCounts = argsPush.segCounts;
    RWStructuredBuffer<uint> args = argsPush.args;

    // each body shows the same fraction of its own orbit; a shared length
    // would make Neptune a stub and Mercury a smear
    const uint wanted = uint(max(0.0,
        bodies[tid].trailPeriodDays * argsPush.orbitTurns / argsPush.dayPerSample
    ));
    const uint usable = min(argsPush.filled, wanted);

    // n samples make n - 1 segments, and that is also what keeps the oldest
    // segment from reaching across the ring seam
    const bool enabled = (argsPush.enabledMask & (1u << tid)) != 0u;
    const uint segCount = (enabled && usable > 1) ? usable - 1 : 0;

    // the vertex stage reads this as the fade denominator, and the marker
    // pass reads it to skip a switched-off body
    segCounts[tid] = segCount;

    args[tid * 4 + 0] = TRAIL_VERTEX_COUNT;
    args[tid * 4 + 1] = segCount;
    args[tid * 4 + 2] = 0;
    // baseInstance is the drawID the vertex stage reads as bodyIdx
    args[tid * 4 + 3] = tid;
}
