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
    class UIRenderer {
    private:
        ImGuiContext* context = nullptr;

        RHIDevice& device;
        RHICapabilities capabilities;
        u32 srgbTarget = 0;

        RHIGraphicsPipelineStateRAII pso;

        RHIBufferSlice vertices;
        RHIBufferSlice indices;

        std::unordered_map<u64, RHITextureRAII> textures;

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

        // the acquire halves of Prepare's texture updates, for the
        // BeginRenderPass that draws the UI
        std::span<const RHITextureBarrier> TextureAcquires() const {
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
