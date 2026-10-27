#include "Sampler.slang"
#include "TerrainDensity.slang"

// The triangle table arrives as a buffer from MarchingCubes.cpp, so it has
// one transcription.

// Mirrors TerrainVertex in Terrain.hpp.
struct TerrainVertex {
    float3 position;
    float3 normal;
};

// Mirrors PushConstants in TerrainMarch.cpp.
struct ResourceData {
    TerrainParams params;
    uint cellsX;
    uint cellsY;
    uint cellsZ;
    float cellSize;
    uint triangleCapacity;
    uint vertexCapacity;
    DescriptorHandle<RWTexture3D<float>> densityWrite;
    DescriptorHandle<Texture3D<float>> densityRead;
    DescriptorHandle<RWStructuredBuffer<TerrainVertex>> vertices;
    // see TerrainMarchCounter in TerrainMarch.hpp for the four slots
    DescriptorHandle<RWStructuredBuffer<uint>> counter;
    DescriptorHandle<StructuredBuffer<int>> triTable;
    // RHIDrawArgs as four uints
    DescriptorHandle<RWStructuredBuffer<uint>> args;
    DescriptorHandle<RWStructuredBuffer<uint>> edgeVertices;
    DescriptorHandle<RWStructuredBuffer<uint>> indices;
    // RHIDrawIndexedArgs as five uints
    DescriptorHandle<RWStructuredBuffer<uint>> argsIndexed;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

static const uint MC_MAX_INDICES = 16;

// Mirrors MC_CORNER_OFFSET and MC_EDGE_CORNERS in MarchingCubes.hpp.
static const uint3 MC_CORNER_OFFSET[8] = {
    uint3(0, 0, 0), uint3(1, 0, 0), uint3(1, 0, 1), uint3(0, 0, 1),
    uint3(0, 1, 0), uint3(1, 1, 0), uint3(1, 1, 1), uint3(0, 1, 1)
};

static const uint2 MC_EDGE_CORNERS[12] = {
    uint2(0, 1), uint2(1, 2), uint2(2, 3), uint2(3, 0),
    uint2(4, 5), uint2(5, 6), uint2(6, 7), uint2(7, 4),
    uint2(0, 4), uint2(1, 5), uint2(2, 6), uint2(3, 7)
};

uint3 cells() {
    return uint3(resource.cellsX, resource.cellsY, resource.cellsZ);
}

uint3 cornerGrid() {
    return cells() + 1;
}

// no vertex welded onto this edge - it does not cross the surface, or it
// ran out of room
static const uint NO_VERTEX = 0xFFFFFFFFu;

uint cornerIndex(uint3 corner) {
    uint3 grid = cornerGrid();

    return (corner.z * grid.y + corner.y) * grid.x + corner.x;
}

uint3 axisStep(uint axis) {
    return uint3(axis == 0 ? 1 : 0, axis == 1 ? 1 : 0, axis == 2 ? 1 : 0);
}

// Each grid edge belongs to the corner at its low end, so it has one vertex.
// Derived from the corner offsets rather than from a second table.
void edgeOwner(int edge, out uint3 corner, out uint axis) {
    uint2 ends = MC_EDGE_CORNERS[edge];
    int3 a = int3(MC_CORNER_OFFSET[ends.x]);
    int3 b = int3(MC_CORNER_OFFSET[ends.y]);

    corner = uint3(min(a, b));

    int3 step = b - a;
    axis = step.x != 0 ? 0 : (step.y != 0 ? 1 : 2);
}

uint edgeSlot(uint3 corner, uint axis) {
    return cornerIndex(corner) * 3 + axis;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_clear() {
    RWStructuredBuffer<uint> counter = resource.counter;

    counter[0] = 0;
    counter[1] = 0;
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

// the table only names edges whose corners straddle zero, so the denominator
// cannot vanish
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
    if (any(tid >= cells()))
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

    // the reservation stands, so the count runs past capacity; record the
    // lowest dropped slot, since the soup is contiguous below it
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

[shader("compute")]
[numthreads(4, 4, 4)]
void cs_edges(uint3 tid: SV_DispatchThreadID) {
    if (any(tid >= cornerGrid()))
        return;

    Texture3D<float> density = resource.densityRead;
    RWStructuredBuffer<TerrainVertex> vertices = resource.vertices;
    RWStructuredBuffer<uint> edgeVertices = resource.edgeVertices;
    RWStructuredBuffer<uint> counter = resource.counter;

    float here = density.Load(int4(int3(tid), 0));

    // every slot is written, so nothing has to clear this buffer first
    for (uint axis = 0; axis < 3; ++axis) {
        uint slot = edgeSlot(tid, axis);

        if (tid[axis] >= cells()[axis]) {
            edgeVertices[slot] = NO_VERTEX;
            continue;
        }

        uint3 far = tid + axisStep(axis);
        float there = density.Load(int4(int3(far), 0));

        if ((here > 0.0f) == (there > 0.0f)) {
            edgeVertices[slot] = NO_VERTEX;
            continue;
        }

        uint index;
        InterlockedAdd(counter[3], 1, index);
        if (index >= resource.vertexCapacity) {
            InterlockedOr(counter[1], 1);
            edgeVertices[slot] = NO_VERTEX;
            continue;
        }

        float3 position = lerp(
            float3(tid) * resource.cellSize,
            float3(far) * resource.cellSize,
            here / (here - there)
        );

        TerrainVertex vertex;
        vertex.position = position;
        vertex.normal = surfaceNormal(density, position);
        vertices[index] = vertex;

        edgeVertices[slot] = index;
    }
}

[shader("compute")]
[numthreads(4, 4, 4)]
void cs_indices(uint3 tid: SV_DispatchThreadID) {
    if (any(tid >= cells()))
        return;

    Texture3D<float> density = resource.densityRead;
    StructuredBuffer<int> triTable = resource.triTable;
    RWStructuredBuffer<uint> edgeVertices = resource.edgeVertices;
    RWStructuredBuffer<uint> indices = resource.indices;
    RWStructuredBuffer<uint> counter = resource.counter;

    uint caseIndex = 0;
    [unroll]
    for (uint i = 0; i < 8; ++i) {
        if (density.Load(int4(int3(tid + MC_CORNER_OFFSET[i]), 0)) > 0.0f)
            caseIndex |= 1u << i;
    }

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

    if (base + triCount > resource.triangleCapacity) {
        InterlockedOr(counter[1], 1);
        InterlockedMin(counter[2], base);
        return;
    }

    for (uint i = 0; i < indexCount; ++i) {
        uint3 corner;
        uint axis;
        edgeOwner(triTable[row + i], corner, axis);

        uint vertex = edgeVertices[edgeSlot(tid + corner, axis)];

        // NO_VERTEX only when cs_edges ran out, which is already flagged; a
        // degenerate triangle beats an out-of-range index
        indices[base * 3 + i] = vertex == NO_VERTEX ? 0 : vertex;
    }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_args() {
    RWStructuredBuffer<uint> counter = resource.counter;
    RWStructuredBuffer<uint> args = resource.args;

    uint drawable = min(counter[0], counter[2]);

    args[0] = drawable * 3;  // vertexCount
    args[1] = 1;             // instanceCount
    args[2] = 0;             // firstVertex
    args[3] = 0;             // baseInstance
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_args_indexed() {
    RWStructuredBuffer<uint> counter = resource.counter;
    RWStructuredBuffer<uint> args = resource.argsIndexed;

    uint drawable = min(counter[0], counter[2]);

    args[0] = drawable * 3;  // indexCount
    args[1] = 1;             // instanceCount
    args[2] = 0;             // firstIndex
    args[3] = 0;             // baseVertex
    args[4] = 0;             // baseInstance
}
