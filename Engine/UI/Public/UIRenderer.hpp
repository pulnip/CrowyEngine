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
        template<typename T>
        struct Retired{
            u64 frame;
            RAII<T> resource;
        };

    private:
        ImGuiContext* context = nullptr;

        RHIDevice& device;
        const u64& frameIndex;
        RHICapabilities capabilities;
        u32 srgbTarget = 0;

        RHIGraphicsPipelineStateRAII pso;

        RHIBufferRAII vertexBuffer;
        RHIBufferRAII indexBuffer;
        u32 vertexCapacity = 0, indexCapacity = 0;

        std::unordered_map<u64, RHITextureRAII> textures;

        std::vector<Retired<RHIBuffer>> retiredBuffers;
        std::vector<Retired<RHITexture>> retiredTextures;

        // acquire halves of this frame's texture-update edges;
        // consumed by the render pass that samples them
        std::vector<RHITextureBarrier> textureAcquires;

    public:
        UIRenderer(RHIDevice&, RHIPixelFormat renderTargetFormat);
        ~UIRenderer();
        CROWY_DECLARE_PINNED(UIRenderer)

        void Prepare(RHICommandList&, Widget&, UIContext&);
        void Record(RHICommandList&);

        // Prepare's texture updates release edges whose acquire halves must
        // ride the render pass that draws the UI - pass these to that
        // BeginRenderPass alongside your own acquires
        std::span<const RHITextureBarrier> TextureAcquires() const{
            return textureAcquires;
        }

    private:
        void setupRenderState(RHICommandList&, Vec2 framebuffer);

        void collectRetired();
        void retire(RHIBufferRAII);
        void retire(RHITextureRAII);

        void createTexture(ImTextureData&);
        void updateTextures(RHICommandList&);
        void updateTexture(RHICommandList&, ImTextureData&);
        void destroyTexture(ImTextureData&);

        void uploadGeometry();
    };
}
