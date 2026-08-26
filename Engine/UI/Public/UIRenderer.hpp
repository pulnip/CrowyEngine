#pragma once

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "Widget.hpp"

struct ImGuiContext;
struct ImTextureData;

namespace Crowy
{
    class UIRenderer{
    private:
        ImGuiContext* context = nullptr;

        RHIDevice& device;
        RHICapabilities capabilities;
        u32 srgbTarget = 0;

        RHIGraphicsPipelineStateRAII pso;

        // this frame's geometry, refreshed by every Prepare()
        RHIBufferSlice vertices;
        RHIBufferSlice indices;

        std::unordered_map<u64, RHITextureRAII> textures;

        // acquire halves of this frame's texture-update edges;
        // consumed by the render pass that samples them
        std::vector<RHITextureBarrier> textureAcquires;

    public:
        // pass the render pass's depth format when the UI is recorded inside
        // a pass that binds one - the PSO formats must match the pass
        UIRenderer(
            RHIDevice&,
            RHIPixelFormat renderTargetFormat,
            RHIPixelFormat depthFormat = RHIPixelFormat::Unknown
        );
        ~UIRenderer();
        CROWY_DECLARE_PINNED(UIRenderer)

        u32 BeginDockSpace();

        void Prepare(RHICommandList&, Widget&, UIContext&);
        // the ImGui frame must end even on a frame that draws no panel;
        // this closes it with nothing submitted
        void Prepare(RHICommandList&);
        void Record(RHICommandList&);

        // Prepare's texture updates release edges whose acquire halves must
        // ride the render pass that draws the UI - pass these to that
        // BeginRenderPass alongside your own acquires
        std::span<const RHITextureBarrier> TextureAcquires() const{
            return textureAcquires;
        }

    private:
        void setupRenderState(RHICommandList&, Vec2 framebuffer);

        void createTexture(ImTextureData&);
        void updateTextures(RHICommandList&);
        void updateTexture(RHICommandList&, ImTextureData&);
        void destroyTexture(ImTextureData&);

        void uploadGeometry();
    };
}
