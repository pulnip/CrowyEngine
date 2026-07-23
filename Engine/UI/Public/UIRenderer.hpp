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

    public:
        UIRenderer(RHIDevice&, RHIPixelFormat renderTargetFormat);
        ~UIRenderer();
        CROWY_DECLARE_PINNED(UIRenderer)

        void Prepare(RHICommandList&, Widget&, UIContext&);
        void Record(RHICommandList&);

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
