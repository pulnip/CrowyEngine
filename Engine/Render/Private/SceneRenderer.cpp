#include "SceneRenderer.hpp"

#include <algorithm>
#include <cmath>
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
    namespace
    {
        // A row as the shaders read it. Each guard keeps a value the port
        // or the panel may write from reaching the shader as NaN or inf.
        LightData packLight(const LightSnapshot& light) {
            using enum LightKind;

            constexpr f32 MinDirectionSquared = 1e-12f;
            constexpr f32 MinRange = 0.001f;
            constexpr f32 MinConeWidth = 0.001f;

            LightData row{
                .position = light.position,
                .direction = normSquared(light.direction) < MinDirectionSquared
                    ? -unitY()
                    : normalize(light.direction),
                .kind = light.kind,
                .color = light.intensity * light.color
            };
            if(light.kind == Directional)
                return row;

            row.invRange = 1.0f / std::max(light.range, MinRange);
            if(light.kind == Spot) {
                const auto cosInner = std::cos(light.innerConeAngle);
                const auto cosOuter = std::cos(light.outerConeAngle);

                row.coneScale =
                    1.0f / std::max(cosInner - cosOuter, MinConeWidth);
                row.coneOffset = -cosOuter * row.coneScale;
            }

            return row;
        }
    }

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

        lightScratch.clear();
        for(const auto& light: scene.Lights().All()) {
            if(light.enabled)
                lightScratch.push_back(packLight(light));
        }

        const auto& environment = scene.Environment();
        for(auto& view: views) {
            view.skyAmbient = toVec4(environment.skyAmbient);
            view.groundAmbient = toVec4(environment.groundAmbient);
        }

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
        if(!lightScratch.empty()) {
            lightSlice = device.UploadTransient(
                std::span<const LightData>(lightScratch),
                static_cast<u32>(sizeof(LightData))
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
        constexpr auto lightStride = static_cast<u32>(sizeof(LightData));

        // one descriptor spans the whole transient buffer; the base index is
        // what points the shader at this frame's rows inside it
        return ScenePush{
            .materials = materialScratch.empty() ?
                0 :
                materialSlice.buffer->GetReadableID(materialStride),
            .materialBase = materialScratch.empty() ?
                0 :
                materialSlice.offset / materialStride,
            .lights = lightScratch.empty() ?
                0 :
                lightSlice.buffer->GetReadableID(lightStride),
            .lightBase = lightScratch.empty() ?
                0 :
                lightSlice.offset / lightStride,
            .lightCount = static_cast<u32>(lightScratch.size())
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
