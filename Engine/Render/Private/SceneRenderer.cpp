#include "SceneRenderer.hpp"

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "Geometry/Frustum3D.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    SceneRenderer::~SceneRenderer() = default;

    SceneRenderer::SceneRenderer(
        RHIDevice& device,
        u32 drawCapacity,
        u32 viewCount
    )
        : drawDataBuffer(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size = static_cast<u32>(sizeof(DrawData) * drawCapacity),
                  .usage = RHIBufferUsage::ShaderResource,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          argsBuffer(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size = static_cast<u32>(
                      sizeof(RHIDrawIndexedArgs) * drawCapacity
                  ),
                  .usage = RHIBufferUsage::IndirectArgument,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          viewCB(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size = static_cast<u32>(sizeof(ViewData) * viewCount),
                  .usage = RHIBufferUsage::ConstantBuffer,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          drawScratch(drawCapacity),
          argsScratch(drawCapacity),
          views(viewCount) {}

    void SceneRenderer::BuildFrame(const RenderScene& scene, u32 viewIndex) {
        CROWY_ASSERT(viewIndex < views.size());

        const auto frustum = makeFrustum3D(views[viewIndex].viewProj);
        const auto primitives = scene.Primitives();

        // Linear over a packed array, no acceleration structure:
        // this loop is what a compute shader replaces
        drawCount = 0;
        for(usize i = 0; i < primitives.size(); ++i) {
            const auto& primitive = primitives[i];

            if(!hasFlag(primitive.flags, PrimitiveFlags::Visible))
                continue;
            if(!OverlapFrustumAABB3D(frustum, primitive.worldBounds))
                continue;

            CROWY_ASSERT(
                drawCount < DrawCapacity(),
                "SceneRenderer ran out of draw rows; raise drawCapacity"
            );

            const auto& geometry = primitive.geometry;
            drawScratch[drawCount] = DrawData{
                .world = primitive.localToWorld,
                .objectID = static_cast<u32>(i)
            };
            argsScratch[drawCount] = RHIDrawIndexedArgs{
                .indexCount = geometry.indexCount,
                .firstIndex = geometry.firstIndex,
                .baseVertex = geometry.baseVertex,
                .baseInstance = drawCount
            };
            ++drawCount;
        }

        uploaded = false;
    }

    void SceneRenderer::Upload() {
        // only the rows this frame filled
        if(drawCount > 0) {
            drawDataBuffer->Upload(
                drawScratch.data(),
                static_cast<u32>(sizeof(DrawData) * drawCount)
            );
            argsBuffer->Upload(
                argsScratch.data(),
                static_cast<u32>(sizeof(RHIDrawIndexedArgs) * drawCount)
            );
        }
        viewCB->Upload(
            views.data(),
            static_cast<u32>(sizeof(ViewData) * views.size())
        );

        uploaded = true;
    }

    u64 SceneRenderer::DrawDataID() {
        CROWY_ASSERT(
            uploaded,
            "DrawDataID() before Upload(): a CPUWrite buffer has no descriptor "
            "for a frame slot that has not been written yet"
        );

        return drawDataBuffer->GetReadableID(
            static_cast<u32>(sizeof(DrawData))
        );
    }

    void SceneRenderer::BindView(
        RHICommandList& cmdList,
        u32 slot,
        u32 viewIndex
    ) const {
        CROWY_ASSERT(viewIndex < views.size());

        cmdList.SetGraphicsConstantBuffer(
            *viewCB,
            slot,
            viewIndex * static_cast<u32>(sizeof(ViewData))
        );
    }

    void SceneRenderer::Submit(
        RHICommandList& cmdList,
        RHIGraphicsPipelineState& pso,
        const RHIIndexBufferView& indices
    ) const {
        if(drawCount == 0)
            return;

        cmdList.ExecuteIndirectIndexed(
            DrawBatchIndexed{
                .pso = &pso,
                .args = argsBuffer.get(),
                .drawCount = drawCount,
                .indices = indices
            }
        );
    }
}
