#include "TerrainDensity.slang"

// Evaluates the density field at a list of points for the
// TerrainDensityCheck sample to hold against the CPU implementation.

struct ResourceData {
    // params leads: two 16-byte rows, so later offsets match C++
    TerrainParams params;               //  0 .. 31
    uint pointCount;                    // 32
    uint _pad0;                         // 36
    DescriptorHandle<StructuredBuffer<float4>> points;      // 40
    DescriptorHandle<RWStructuredBuffer<float2>> results;   // 48
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint tid: SV_DispatchThreadID) {
    // the dispatch rounds the thread count up to the group size
    if (tid >= resource.pointCount)
        return;

    StructuredBuffer<float4> points = resource.points;
    RWStructuredBuffer<float2> results = resource.results;

    float3 position = points[tid].xyz;

    // reported separately so a mismatch points at fbm or cave noise
    float baseHeight = terrainBaseHeight(position.x, position.z, resource.params);
    float density = terrainDensity(
        position,
        baseHeight,
        resource.params
    );

    results[tid] = float2(baseHeight, density);
}
