#include "SceneRenderer.hpp"

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "Geometry/Frustum3D.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    SceneRenderer::~SceneRenderer() = default;

    SceneRenderer::SceneRenderer(
        RHIDevice& device,
        const SceneRendererDesc& desc
    )
        : device(device),
          pipelines(device),
          drawScratch(desc.drawCapacity),
          argsScratch(desc.drawCapacity),
          materialScratch(desc.materialCapacity),
          views(desc.viewCount) {}

    u32 SceneRenderer::bucketOf(RHIGraphicsPipelineState* pso) {
        for(u32 i = 0; i < buckets.size(); ++i) {
            if(buckets[i].pso == pso)
                return i;
        }

        buckets.push_back(DrawBucket{.pso = pso});

        return static_cast<u32>(buckets.size() - 1);
    }

    void SceneRenderer::resolvePipelines(
        const RenderScene& scene,
        const PassPipelineDesc& pass
    ) {
        const auto& materials = scene.Materials();

        materialCount = static_cast<u32>(materials.Count());
        CROWY_ASSERT(
            materialCount <= materialScratch.size(),
            "SceneRenderer ran out of material rows; raise materialCapacity"
        );

        pipelineOfMaterial.resize(materialCount);
        for(u32 i = 0; i < materialCount; ++i) {
            const auto& material = materials.At(i);

            materialScratch[i] = material.data;
            // once per material per pass, never inside the draw loop
            pipelineOfMaterial[i] = &pipelines.Resolve(material.pipeline, pass);
        }
    }

    void SceneRenderer::BuildFrame(
        const RenderScene& scene,
        const PassPipelineDesc& pass,
        u32 viewIndex
    ) {
        CROWY_ASSERT(viewIndex < views.size());

        resolvePipelines(scene, pass);

        const auto& materials = scene.Materials();
        const auto& meshes = scene.Meshes();
        const auto primitives = scene.Primitives().All();
        const auto frustum = makeFrustum3D(views[viewIndex].viewProj);

        // Linear over a packed array, no acceleration structure:
        // this loop is what a compute shader replaces
        visibleScratch.clear();
        buckets.clear();
        for(usize i = 0; i < primitives.size(); ++i) {
            const auto& primitive = primitives[i];

            if(!hasFlag(primitive.flags, PrimitiveFlags::Visible))
                continue;
            if(!OverlapFrustumAABB3D(frustum, primitive.worldBounds))
                continue;

            const auto& mesh = meshes.Read(primitive.mesh);
            for(const auto& subMesh: mesh.subMeshes) {
                const auto material = mesh.materials[subMesh.materialSlot];
                const auto materialIndex =
                    static_cast<u32>(materials.IndexOf(material));

                const auto bucket = bucketOf(pipelineOfMaterial[materialIndex]);
                ++buckets[bucket].drawCount;

                visibleScratch.push_back(
                    VisibleDraw{
                        .geometry = subMesh.geometry,
                        .bucket = bucket,
                        .primitive = static_cast<u32>(i),
                        .materialIndex = materialIndex
                    }
                );
            }
        }

        CROWY_ASSERT(
            visibleScratch.size() <= DrawCapacity(),
            "SceneRenderer ran out of draw rows; raise drawCapacity"
        );

        // Each bucket becomes one contiguous run of the args buffer,
        // so the offsets have to be known before any row is written.
        u32 offset = 0;
        for(auto& bucket: buckets) {
            bucket.firstDraw = offset;
            offset += bucket.drawCount;
            // reused as a write cursor below, then restored
            bucket.drawCount = 0;
        }

        for(const auto& visible: visibleScratch) {
            auto& bucket = buckets[visible.bucket];
            const auto slot = bucket.firstDraw + bucket.drawCount;
            ++bucket.drawCount;

            drawScratch[slot] = DrawData{
                .world = primitives[visible.primitive].localToWorld,
                .materialIndex = visible.materialIndex,
                .objectID = visible.primitive,
                .vbIndex = static_cast<u32>(visible.geometry.baseVertex)
            };
            argsScratch[slot] = RHIDrawIndexedArgs{
                .indexCount = visible.geometry.indexCount,
                .firstIndex = visible.geometry.firstIndex,
                // the pool offset rides in vbIndex instead, because
                // SV_VertexID picks this up on Metal but not on D3D12
                .baseVertex = 0,
                // the row index is global, not bucket-relative
                .baseInstance = slot
            };
        }

        drawCount = offset;
        uploaded = false;
    }

    void SceneRenderer::Upload() {
        // only the rows this frame filled
        if(drawCount > 0) {
            drawDataSlice = device.UploadTransient(
                std::span<const DrawData>(drawScratch.data(), drawCount),
                static_cast<u32>(sizeof(DrawData))
            );
            argsSlice = device.UploadTransient(
                std::span<const RHIDrawIndexedArgs>(
                    argsScratch.data(), drawCount
                ),
                static_cast<u32>(sizeof(RHIDrawIndexedArgs))
            );
        }
        if(materialCount > 0) {
            materialSlice = device.UploadTransient(
                std::span<const MaterialData>(
                    materialScratch.data(), materialCount
                ),
                static_cast<u32>(sizeof(MaterialData))
            );
        }
        viewSlice = device.UploadTransient(
            std::span<const ViewData>(views),
            RHI_CB_ALIGN
        );

        uploaded = true;
    }

    ScenePush SceneRenderer::Push() {
        CROWY_ASSERT(
            uploaded,
            "Push() before Upload(): this frame's rows have no slice yet"
        );

        constexpr auto drawStride = static_cast<u32>(sizeof(DrawData));
        constexpr auto materialStride = static_cast<u32>(sizeof(MaterialData));

        // one descriptor spans the whole transient buffer; the base index is
        // what points the shader at this frame's rows inside it
        return ScenePush{
            .draws = drawCount > 0 ?
                drawDataSlice.buffer->GetReadableID(drawStride) : 0,
            .materials = materialCount > 0 ?
                materialSlice.buffer->GetReadableID(materialStride) : 0,
            .drawBase = drawCount > 0 ?
                drawDataSlice.offset / drawStride : 0,
            .materialBase = materialCount > 0 ?
                materialSlice.offset / materialStride : 0
        };
    }

    void SceneRenderer::BindView(
        RHICommandList& cmdList,
        u32 slot,
        u32 viewIndex
    ) const {
        CROWY_ASSERT(viewIndex < views.size());

        cmdList.SetGraphicsConstantBuffer(
            *viewSlice.buffer,
            slot,
            viewSlice.offset + viewIndex * static_cast<u32>(sizeof(ViewData))
        );
    }

    void SceneRenderer::Submit(
        RHICommandList& cmdList,
        const RHIIndexBufferView& indices
    ) const {
        for(const auto& bucket: buckets) {
            if(bucket.drawCount == 0)
                continue;

            cmdList.ExecuteIndirectIndexed(
                DrawBatchIndexed{
                    .pso = bucket.pso,
                    .args = argsSlice.buffer,
                    .argsOffset = argsSlice.offset +
                        bucket.firstDraw * sizeof(RHIDrawIndexedArgs),
                    .drawCount = bucket.drawCount,
                    .indices = indices
                }
            );
        }
    }
}
