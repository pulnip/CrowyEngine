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

    // one of RHI_STATIC_SAMPLERS, in its order; SampleMap mirrors it
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

    // an uploaded texture and its sampler: one decision with its mips, since
    // a nearest sampler still walks mips a texture has
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
