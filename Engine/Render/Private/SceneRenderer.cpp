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
        const SceneRendererDesc& desc
    )
        : drawDataBuffer(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size =
                      static_cast<u32>(sizeof(DrawData) * desc.drawCapacity),
                  .usage = RHIBufferUsage::ShaderResource,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          argsBuffer(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size = static_cast<u32>(
                      sizeof(RHIDrawIndexedArgs) * desc.drawCapacity
                  ),
                  .usage = RHIBufferUsage::IndirectArgument,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          materialBuffer(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size = static_cast<u32>(
                      sizeof(MaterialData) * desc.materialCapacity
                  ),
                  .usage = RHIBufferUsage::ShaderResource,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          viewCB(device.CreateBuffer(
              RHIBufferCreateDesc{
                  .size = static_cast<u32>(sizeof(ViewData) * desc.viewCount),
                  .usage = RHIBufferUsage::ConstantBuffer,
                  .access = RHIMemoryAccess::CPUWrite
              }
          )),
          drawScratch(desc.drawCapacity),
          argsScratch(desc.drawCapacity),
          materialScratch(desc.materialCapacity),
          views(desc.viewCount) {}

    void SceneRenderer::BuildFrame(const RenderScene& scene, u32 viewIndex) {
        CROWY_ASSERT(viewIndex < views.size());

        const auto& materials = scene.Materials();
        const auto& meshes = scene.Meshes();
        const auto primitives = scene.Primitives().All();

        materialCount = static_cast<u32>(materials.Count());
        CROWY_ASSERT(
            materialCount <= materialScratch.size(),
            "SceneRenderer ran out of material rows; raise materialCapacity"
        );
        for(u32 i = 0; i < materialCount; ++i) {
            materialScratch[i] = materials.At(i).data;
        }

        const auto frustum = makeFrustum3D(views[viewIndex].viewProj);

        // Linear over a packed array, no acceleration structure:
        // this loop is what a compute shader replaces
        drawCount = 0;
        for(usize i = 0; i < primitives.size(); ++i) {
            const auto& primitive = primitives[i];

            if(!hasFlag(primitive.flags, PrimitiveFlags::Visible))
                continue;
            if(!OverlapFrustumAABB3D(frustum, primitive.worldBounds))
                continue;

            const auto& mesh = meshes.Read(primitive.mesh);
            for(const auto& subMesh: mesh.subMeshes) {
                CROWY_ASSERT(
                    drawCount < DrawCapacity(),
                    "SceneRenderer ran out of draw rows; raise drawCapacity"
                );

                const auto material = mesh.materials[subMesh.materialSlot];
                drawScratch[drawCount] = DrawData{
                    .world = primitive.localToWorld,
                    .materialIndex =
                        static_cast<u32>(materials.IndexOf(material)),
                    .objectID = static_cast<u32>(i)
                };
                argsScratch[drawCount] = RHIDrawIndexedArgs{
                    .indexCount = subMesh.geometry.indexCount,
                    .firstIndex = subMesh.geometry.firstIndex,
                    .baseVertex = subMesh.geometry.baseVertex,
                    .baseInstance = drawCount
                };
                ++drawCount;
            }
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
        if(materialCount > 0) {
            materialBuffer->Upload(
                materialScratch.data(),
                static_cast<u32>(sizeof(MaterialData) * materialCount)
            );
        }
        viewCB->Upload(
            views.data(),
            static_cast<u32>(sizeof(ViewData) * views.size())
        );

        uploaded = true;
    }

    ScenePush SceneRenderer::Push() {
        CROWY_ASSERT(
            uploaded,
            "Push() before Upload(): a CPUWrite buffer has no descriptor for a "
            "frame slot that has not been written yet"
        );

        return ScenePush{
            .draws = drawDataBuffer->GetReadableID(
                static_cast<u32>(sizeof(DrawData))
            ),
            .materials = materialBuffer->GetReadableID(
                static_cast<u32>(sizeof(MaterialData))
            )
        };
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
