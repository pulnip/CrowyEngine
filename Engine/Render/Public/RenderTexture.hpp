#pragma once

#include <type_traits>

#include "PackedTable.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    struct TextureResource;

    using TextureHandle = GenericHandle<TextureResource>;
    using TextureTable = PackedTable<TextureResource>;

    // which of the static samplers a texture is read with, in
    // RHI_STATIC_SAMPLERS order; mirrored by SampleMap in
    // Engine/Shader/ForwardShading.slang
    enum class TextureSampler : u32 {
        LinearWrap,
        LinearClamp,
        LinearMirror,
        LinearBorder,
        NearestWrap,
        NearestClamp,
        NearestMirror,
        NearestBorder,
    };
    static_assert(
        static_cast<usize>(TextureSampler::NearestBorder) + 1 == RHI_STATIC_SAMPLERS.size()
    );

    // An uploaded texture and how it is filtered: one decision with its mip
    // chain, since a nearest sampler still walks mips a texture has. sRGB or
    // linear is the texture's format.
    struct TextureResource {
        RHITextureRAII texture;
        TextureSampler sampler = TextureSampler::LinearClamp;
    };

    // One table row as the forward program reads it, mirrored in
    // Engine/Shader/SceneData.slang.
    struct TextureData {
        // DescriptorHandle<Texture2D>
        u64 texture = 0;
        u32 sampler = 0;
        u32 _pad = 0;
    };
    static_assert(sizeof(TextureData) == 16);
    static_assert(std::is_trivially_copyable_v<TextureData>);
}
