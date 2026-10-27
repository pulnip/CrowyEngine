#include "RHIDevice.hpp"

#include <utility>

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    RHIBufferRAII RHIDevice::CreateBuffer(
        const RHIBufferCreateDesc& desc,
        StrView name
    ) {
        using enum RHIMemoryType;

        CROWY_ASSERT(desc.size % 4 == 0, "a buffer holds whole 4-byte words");
        CROWY_ASSERT(
            desc.memory != Transient,
            "tile memory holds render targets, not buffers"
        );
        CROWY_ASSERT(
            desc.memory == GPUOnly || !desc.shaderWrite,
            "a shader cannot write CPU-visible memory"
        );

        return createBuffer(desc, name);
    }

    RHITextureRAII RHIDevice::CreateTexture(
        const RHITextureCreateDesc& desc,
        StrView name
    ) {
        using enum RHITextureUsage;

        const bool shaderResource = hasFlag(desc.usage, ShaderResource);
        const bool attachment = hasFlag(desc.usage, RenderTarget) ||
                                hasFlag(desc.usage, DepthStencil);
        const bool is3D = desc.depth > 1;

        CROWY_ASSERT(
            desc.memory != RHIMemoryType::CPUWrite &&
                desc.memory != RHIMemoryType::CPURead,
            "a texture is never CPU-visible; use a buffer"
        );
        CROWY_ASSERT(
            desc.height >= 1 && desc.depth >= 1,
            "a texture is at least one texel high and deep"
        );
        CROWY_ASSERT(
            !isBlockCompressed(desc.format) || shaderResource,
            "Block-compressed textures are shader-resource only"
        );
        CROWY_ASSERT(
            !isDepthFormat(desc.format) || hasFlag(desc.usage, DepthStencil),
            "Depth format should be depth stencil usage"
        );
        CROWY_ASSERT(
            !is3D || desc.arraySize == 1,
            "3D textures cannot be arrayed"
        );
        CROWY_ASSERT(
            !is3D || !attachment,
            "3D render/depth targets are not supported"
        );
        // RHISubresourceData carries no slice pitch
        CROWY_ASSERT(
            desc.initialData.empty() || desc.depth == 1,
            "initial data upload is not supported for 3D textures"
        );
        CROWY_ASSERT(
            desc.initialData.empty() ||
                desc.initialData.size() == desc.mipLevels * desc.arraySize,
            "initial data holds one entry per mip and array slice"
        );

        return createTexture(desc, name);
    }

    void RHIDevice::Retire(RHIBufferRAII buffer) {
        if(buffer == nullptr)
            return;

        DeferRetire([buffer = std::move(buffer)] {});
    }

    void RHIDevice::Retire(RHITextureRAII texture) {
        if(texture == nullptr)
            return;

        DeferRetire([texture = std::move(texture)] {});
    }

    void RHIDevice::Retire(RHIGraphicsPipelineStateRAII state) {
        if(state == nullptr)
            return;

        DeferRetire([state = std::move(state)] {});
    }
}
