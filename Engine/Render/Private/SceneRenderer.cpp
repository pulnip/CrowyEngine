#include "SceneRenderer.hpp"

#include <span>

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "Geometry/Frustum3D.hpp"
#include "LinearAlgebra.hpp"
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
        : device(device),
          pipelines(device),
          views(desc.viewCount),
          culls(desc.viewCount) {
        materialScratch.reserve(desc.materialCapacity);
    }

    void SceneRenderer::BeginFrame(const RenderScene& scene) {
        materialScratch.clear();
        for(const auto& material: scene.Materials().All())
            materialScratch.push_back(material.data);

        for(auto& cull: culls)
            cull.culled = false;
        frameScene = &scene;
        uploaded = false;
    }

    const VisibleSet& SceneRenderer::Visible(u32 viewIndex) {
        CROWY_ASSERT(viewIndex < views.size());
        CROWY_ASSERT(frameScene != nullptr, "Visible() before BeginFrame()");

        auto& cull = culls[viewIndex];
        if(cull.culled)
            return cull.visible;
        cull.culled = true;

        const auto& materials = frameScene->Materials();
        const auto& meshes = frameScene->Meshes();
        const auto& primitives = frameScene->Primitives();
        const auto& viewProj = views[viewIndex].viewProj;
        const auto frustum = makeFrustum3D(viewProj);

        auto& visible = cull.visible;
        visible.draws.clear();
        visible.primitiveCount = 0;
        // Linear over a packed array, no acceleration structure:
        // this loop is what a compute shader replaces
        for(usize i = 0; i < primitives.Count(); ++i) {
            const auto& primitive = primitives.At(i);

            if(!hasFlag(primitive.flags, PrimitiveFlags::Visible))
                continue;
            if(!OverlapFrustumAABB3D(frustum, primitive.worldBounds))
                continue;
            ++visible.primitiveCount;

            // before the divide, clip z grows with view z under a perspective
            // and an orthographic view alike
            const auto depth =
                (viewProj * toVec4(primitive.worldBounds.center, 1.0f)).z;
            const auto identity =
                static_cast<u32>(primitives.HandleAt(i).GetIndex());

            const auto& mesh = meshes.GetRef(primitive.mesh);
            for(const auto& subMesh: mesh.subMeshes) {
                const auto material = mesh.materials[subMesh.materialSlot];

                visible.draws.push_back(
                    VisibleDraw{
                        .geometry = subMesh.geometry,
                        .primitive = static_cast<u32>(i),
                        .identity = identity,
                        .materialIndex =
                            static_cast<u32>(materials.IndexOf(material)),
                        .depth = depth,
                        .flags = primitive.flags
                    }
                );
            }
        }

        return visible;
    }

    void SceneRenderer::Upload() {
        if(!materialScratch.empty()) {
            materialSlice = device.UploadTransient(
                std::span<const MaterialData>(materialScratch),
                static_cast<u32>(sizeof(MaterialData))
            );
        }
        viewSlice = device.UploadTransient(
            std::span<const ViewData>(views),
            RHI_CB_ALIGN
        );

        uploaded = true;
    }

    ScenePush SceneRenderer::FramePush() const {
        CROWY_ASSERT(
            uploaded,
            "FramePush() before Upload(): the materials have no slice yet"
        );

        constexpr auto materialStride = static_cast<u32>(sizeof(MaterialData));

        // one descriptor spans the whole transient buffer; the base index is
        // what points the shader at this frame's rows inside it
        return ScenePush{
            .materials = materialScratch.empty() ?
                0 :
                materialSlice.buffer->GetReadableID(materialStride),
            .materialBase = materialScratch.empty() ?
                0 :
                materialSlice.offset / materialStride
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
}
