#pragma once

#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "Terrain.hpp"

namespace Crowy
{
    // Soup emits three vertices per triangle, drawn unindexed; Welded emits
    // one per crossing edge and indexes it, about a third of the soup's bytes.
    enum class TerrainMarchMode : u8 { Soup, Welded };

    struct TerrainMarchTargets {
        RHIResourceUsage vertices = RHIResourceUsage::SampledVertex;
        // nothing outside the compute passes reads it, but the release still
        // stands: next frame's clear is a write-after-write
        RHIResourceUsage counter = RHIResourceUsage::StorageCompute;
        RHIResourceUsage args = RHIResourceUsage::IndirectArgs;
        // Welded mode only
        RHIResourceUsage indices = RHIResourceUsage::IndexBuffer;
    };

    // The release halves a run emitted; the consuming pass acquires with these
    // same values, which is what pairs the two ends of an edge.
    struct TerrainMarchEdges {
        RHIBufferBarrier vertices;
        RHIBufferBarrier counter;
        RHIBufferBarrier args;
        // Welded mode only
        RHIBufferBarrier indices;
    };

    class TerrainMarcher {
    private:
        u32 triangleCapacity;

        RHIComputePipelineStateRAII clearPSO, densityPSO;
        RHIComputePipelineStateRAII marchPSO, argsPSO;
        RHIComputePipelineStateRAII edgesPSO, indicesPSO, argsIndexedPSO;

        RHITextureRAII densityTexture;
        RHIBufferRAII vertexBuffer;
        RHIBufferRAII counterBuffer;
        RHIBufferRAII triTableBuffer;
        RHIBufferRAII argsBuffer;
        RHIBufferRAII edgeVertexBuffer;
        RHIBufferRAII indexBuffer;
        RHIBufferRAII argsIndexedBuffer;

        // the usage the previous submission left each resource in
        RHIResourceUsage densityResting = RHIResourceUsage::Undefined;
        RHIResourceUsage vertexResting = RHIResourceUsage::Undefined;
        RHIResourceUsage counterResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsIndexedResting = RHIResourceUsage::Undefined;
        RHIResourceUsage indexResting = RHIResourceUsage::Undefined;
        RHIResourceUsage edgeVertexResting = RHIResourceUsage::Undefined;

    public:
        TerrainMarcher(RHIDevice&, u32 triangleCapacity);
        // the RAII members hold types this header only forward-declares
        ~TerrainMarcher();

        u32 TriangleCapacity() const noexcept { return triangleCapacity; }
        u32 VertexCapacity() const noexcept { return triangleCapacity * 3; }

        RHIBuffer& Vertices() noexcept { return *vertexBuffer; }
        RHIBuffer& Counter() noexcept { return *counterBuffer; }
        RHIBuffer& Indices() noexcept { return *indexBuffer; }

        // separate buffers: RHIDrawArgs and RHIDrawIndexedArgs differ in layout
        RHIBuffer& Args(TerrainMarchMode mode) noexcept {
            return mode == TerrainMarchMode::Soup ? *argsBuffer
                                                  : *argsIndexedBuffer;
        }

        TerrainMarchEdges Record(
            RHICommandList&,
            const TerrainParams&,
            TerrainMarchMode = TerrainMarchMode::Soup,
            TerrainMarchTargets = {}
        );
    };

    // Mirrors the four slots cs_clear sets in TerrainMarch.slang.
    struct TerrainMarchCounter {
        // reservations handed out, which runs past capacity once it overflows
        u32 triangleCount = 0;
        u32 overflowed = 0;
        // the first slot a dropped cell would have filled; the soup is
        // contiguous below it. All ones while nothing has been dropped
        u32 firstDropped = 0;
        // Welded mode only; Soup leaves it at zero
        u32 vertexCount = 0;

        u32 DrawableTriangles() const noexcept {
            return triangleCount < firstDropped ? triangleCount : firstDropped;
        }
    };
    static_assert(sizeof(TerrainMarchCounter) == 16);
}
