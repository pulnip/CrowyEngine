#include "OrbitCommon.slang"

// Apart from OrbitSample.slang: Metal compiles a module's entry points
// together, and two push constants in one module collide on buffer(0).

struct OrbitArgsPush{
    DescriptorHandle<StructuredBuffer<BodyDraw>> bodies;    //  0
    // RHIDrawArgs[bodyCount], four uints each
    DescriptorHandle<RWStructuredBuffer<uint>> args;        //  8
    DescriptorHandle<RWStructuredBuffer<uint>> segCounts;   // 16
    uint bodyCount;                                         // 24
    uint filled;                                            // 28
    uint enabledMask;                                       // 32
    float orbitTurns;                                       // 36
    float dayPerSample;                                     // 40
    uint _pad0;                                             // 44
};

[[vk::push_constant]]
ConstantBuffer<OrbitArgsPush> argsPush : register(b0);

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

    // n samples make n - 1 segments, which also keeps the oldest segment off
    // the ring seam
    const bool enabled = (argsPush.enabledMask & (1u << tid)) != 0u;
    const uint segCount = (enabled && usable > 1) ? usable - 1 : 0;

    segCounts[tid] = segCount;

    args[tid * 4 + 0] = TRAIL_VERTEX_COUNT;
    args[tid * 4 + 1] = segCount;
    args[tid * 4 + 2] = 0;
    // baseInstance is the drawID the vertex stage reads as bodyIdx
    args[tid * 4 + 3] = tid;
}
