#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    // What one frame asked the RHI to do, counted at the backend-neutral layer.
    // The point is the shape of these numbers against the scene:
    // work that scales with the draw count instead of the pass count is
    // the D3D11 immediate-context habit leaking into a D3D12 backend.
    struct RHIFrameStats{
        // one per recorded command list. on DX12 this is also how often
        // the descriptor heaps and the root signatures get rebound
        u32 commandListBeginCount = 0;
        // pool misses. steady state must be 0 - anything else means a
        // command list and its allocators are built every single frame
        u32 commandListCreateCount = 0;

        u32 renderPassCount = 0;
        u32 computePassCount = 0;
        u32 blitPassCount = 0;

        u32 drawCount = 0;
        u32 indirectBatchCount = 0;
        u32 indirectDrawCount = 0;
        u32 dispatchCount = 0;
        u32 copyCount = 0;

        u32 pipelineSetCount = 0;
        u32 constantBufferSetCount = 0;
        u32 pushConstantSetCount = 0;
        u32 vertexBufferSetCount = 0;

        // edges the frontend asked for, not the barriers a backend emits:
        // DX12 fuses adjacent ones. O(pass) is healthy, O(draw) is not
        u32 barrierEdgeCount = 0;

        RHIFrameStats& operator+=(const RHIFrameStats& rhs) noexcept{
            commandListBeginCount += rhs.commandListBeginCount;
            commandListCreateCount += rhs.commandListCreateCount;

            renderPassCount += rhs.renderPassCount;
            computePassCount += rhs.computePassCount;
            blitPassCount += rhs.blitPassCount;

            drawCount += rhs.drawCount;
            indirectBatchCount += rhs.indirectBatchCount;
            indirectDrawCount += rhs.indirectDrawCount;
            dispatchCount += rhs.dispatchCount;
            copyCount += rhs.copyCount;

            pipelineSetCount += rhs.pipelineSetCount;
            constantBufferSetCount += rhs.constantBufferSetCount;
            pushConstantSetCount += rhs.pushConstantSetCount;
            vertexBufferSetCount += rhs.vertexBufferSetCount;

            barrierEdgeCount += rhs.barrierEdgeCount;

            return *this;
        }
    };
}

// Counting helpers for RHICommandList, which owns a member named `stats`.
// Off, they leave nothing behind: no member, no reset, no increment.
#if CROWY_BENCHMARK
    #define CROWY_STAT(field)        (++(stats.field))
    #define CROWY_STAT_ADD(field, n) ((stats.field) += static_cast<Crowy::u32>(n))
#else
    #define CROWY_STAT(field)        ((void)0)
    #define CROWY_STAT_ADD(field, n) ((void)0)
#endif
