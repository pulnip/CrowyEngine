#include "Sampler.slang"
#include "TerrainDensity.slang"

// Marching cubes over the density field, entirely on the GPU.
//
// cs_density fills the corner grid, cs_march reads it back as a texture and
// emits a triangle soup. Mirrors Engine/RHI/Sample/Terrain/TerrainMarch.hpp;
// the tables come from MarchingCubes.hpp as a buffer, so the two sides share
// one transcription.

// Mirrors TerrainVertex in Engine/RHI/Sample/Terrain/Terrain.hpp.
struct TerrainVertex {
    float3 position;
    float3 normal;
};

// Mirrors PushConstants in TerrainMarch.cpp; padding spelled out on both
// sides so the offsets can be asserted there.
struct ResourceData {
    TerrainParams params;                                          //  0 .. 31
    uint3 cells;                                                   // 32 .. 43
    float cellSize;                                                // 44
    // in triangles; cs_march drops a cell rather than run past it
    uint triangleCapacity;                                         // 48
    uint _pad0;                                                    // 52
    DescriptorHandle<RWTexture3D<float>> densityWrite;             // 56
    DescriptorHandle<Texture3D<float>> densityRead;                // 64
    DescriptorHandle<RWStructuredBuffer<TerrainVertex>> vertices;  // 72
    // see TerrainMarchCounter in TerrainMarch.hpp for the four slots
    DescriptorHandle<RWStructuredBuffer<uint>> counter;            // 80
    DescriptorHandle<StructuredBuffer<int>> triTable;              // 88
    // RHIDrawArgs as four uints; cs_args fills it for ExecuteIndirect
    DescriptorHandle<RWStructuredBuffer<uint>> args;               // 96
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

static const uint MC_MAX_INDICES = 16;

// see MarchingCubes.hpp for the corner numbering these belong to
static const uint3 MC_CORNER_OFFSET[8] = {
    uint3(0, 0, 0), uint3(1, 0, 0), uint3(1, 0, 1), uint3(0, 0, 1),
    uint3(0, 1, 0), uint3(1, 1, 0), uint3(1, 1, 1), uint3(0, 1, 1)
};

static const uint2 MC_EDGE_CORNERS[12] = {
    uint2(0, 1), uint2(1, 2), uint2(2, 3), uint2(3, 0),
    uint2(4, 5), uint2(5, 6), uint2(6, 7), uint2(7, 4),
    uint2(0, 4), uint2(1, 5), uint2(2, 6), uint2(3, 7)
};

uint3 cornerGrid() {
    return resource.cells + 1;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_clear() {
    RWStructuredBuffer<uint> counter = resource.counter;

    counter[0] = 0;
    counter[1] = 0;
    // no cell has been dropped yet, so every slot is still contiguous
    counter[2] = 0xFFFFFFFFu;
    counter[3] = 0;
}

[shader("compute")]
[numthreads(4, 4, 4)]
void cs_density(uint3 tid: SV_DispatchThreadID) {
    // the dispatch rounds the thread count up to the group size
    if (any(tid >= cornerGrid()))
        return;

    RWTexture3D<float> density = resource.densityWrite;

    float3 position = float3(tid) * resource.cellSize;
    float baseHeight = terrainBaseHeight(position.x, position.z, resource.params);

    density[tid] = terrainDensity(position, baseHeight, resource.params);
}

// the density field between corners, as the hardware interpolates it
float sampleDensity(Texture3D<float> density, float3 position) {
    float3 texel = position / resource.cellSize + 0.5f;

    return density.SampleLevel(linearClamp, texel / float3(cornerGrid()), 0);
}

// density climbs towards solid, so the outward normal runs down the gradient
float3 surfaceNormal(Texture3D<float> density, float3 position) {
    float h = resource.cellSize;
    float3 dx = float3(h, 0, 0);
    float3 dy = float3(0, h, 0);
    float3 dz = float3(0, 0, h);

    float3 gradient = float3(
        sampleDensity(density, position + dx) - sampleDensity(density, position - dx),
        sampleDensity(density, position + dy) - sampleDensity(density, position - dy),
        sampleDensity(density, position + dz) - sampleDensity(density, position - dz)
    );

    // a vanishing gradient means a locally flat field, where no direction is
    // more outward than another; +Y keeps the vertex from carrying a NaN
    float len = length(gradient);

    return len > 1e-12f ? -gradient / len : float3(0, 1, 0);
}

// where the isosurface crosses the edge, by linear interpolation.
// the table only names edges whose corners disagree, so the two densities
// straddle zero and the denominator cannot vanish
float3 edgeVertex(uint3 cell, int edge, float corner[8]) {
    uint a = MC_EDGE_CORNERS[edge].x;
    uint b = MC_EDGE_CORNERS[edge].y;

    float3 pa = float3(cell + MC_CORNER_OFFSET[a]) * resource.cellSize;
    float3 pb = float3(cell + MC_CORNER_OFFSET[b]) * resource.cellSize;

    return lerp(pa, pb, corner[a] / (corner[a] - corner[b]));
}

[shader("compute")]
[numthreads(4, 4, 4)]
void cs_march(uint3 tid: SV_DispatchThreadID) {
    if (any(tid >= resource.cells))
        return;

    Texture3D<float> density = resource.densityRead;
    StructuredBuffer<int> triTable = resource.triTable;
    RWStructuredBuffer<TerrainVertex> vertices = resource.vertices;
    RWStructuredBuffer<uint> counter = resource.counter;

    float corner[8];
    uint caseIndex = 0;
    [unroll]
    for (uint i = 0; i < 8; ++i) {
        corner[i] = density.Load(int4(int3(tid + MC_CORNER_OFFSET[i]), 0));
        if (corner[i] > 0.0f)
            caseIndex |= 1u << i;
    }

    // wholly solid or wholly air: no surface passes through
    if (caseIndex == 0 || caseIndex == 255)
        return;

    uint row = caseIndex * MC_MAX_INDICES;
    uint indexCount = 0;
    [unroll]
    for (uint i = 0; i < MC_MAX_INDICES; ++i) {
        if (triTable[row + i] >= 0)
            ++indexCount;
    }
    uint triCount = indexCount / 3;

    uint base;
    InterlockedAdd(counter[0], triCount, base);

    // the reservation stands either way, so the count keeps growing past
    // capacity. A dropped cell leaves its slots unwritten, so record the
    // lowest one: below that the soup is still contiguous
    if (base + triCount > resource.triangleCapacity) {
        InterlockedOr(counter[1], 1);
        InterlockedMin(counter[2], base);
        return;
    }

    for (uint t = 0; t < triCount; ++t) {
        for (uint k = 0; k < 3; ++k) {
            float3 position = edgeVertex(tid, triTable[row + t * 3 + k], corner);

            TerrainVertex vertex;
            vertex.position = position;
            vertex.normal = surfaceNormal(density, position);

            vertices[(base + t) * 3 + k] = vertex;
        }
    }
}

// Turns the count the marching pass accumulated into draw arguments, so the
// vertex count never has to reach the CPU. One thread, run after cs_march.
[shader("compute")]
[numthreads(1, 1, 1)]
void cs_args() {
    RWStructuredBuffer<uint> counter = resource.counter;
    RWStructuredBuffer<uint> args = resource.args;

    // reservations run past capacity once it overflows, and a dropped cell
    // leaves its slots unwritten, so the contiguous mesh ends at the earlier
    // of the two
    uint drawable = min(counter[0], counter[2]);

    args[0] = drawable * 3;  // vertexCount
    args[1] = 1;             // instanceCount
    args[2] = 0;             // firstVertex
    args[3] = 0;             // baseInstance
}
