#pragma once

#include <type_traits>

#include "EnumUtil.hpp"
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

    // one of StaticSamplers, in its order; sampleMap mirrors it
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
        static_cast<usize>(TextureSampler::NearestBorder) + 1 ==
        StaticSamplers.size()
    );

    // clang-format off
    CROWY_ENUM_BEGIN(TextureSampler)
        CROWY_ENUM_VALUE(LinearWrap)
        CROWY_ENUM_VALUE(LinearClamp)
        CROWY_ENUM_VALUE(LinearMirror)
        CROWY_ENUM_VALUE(LinearBorder)
        CROWY_ENUM_VALUE(NearestWrap)
        CROWY_ENUM_VALUE(NearestClamp)
        CROWY_ENUM_VALUE(NearestMirror)
        CROWY_ENUM_VALUE(NearestBorder)
    CROWY_ENUM_END()
    // clang-format on

    // a nearest sampler still walks the mips it is given
    [[nodiscard]] inline constexpr bool isNearest(TextureSampler sampler) {
        return sampler >= TextureSampler::NearestWrap;
    }

    // One table row: a texture as one slot reads it. A caller that owns the
    // texture puts it in `texture`; a slot of an asset's texture leaves
    // that empty and gives the id of its view in `readable`.
    struct TextureResource {
        RHITextureRAII texture;
        TextureSampler sampler = TextureSampler::LinearClamp;
        u64 readable = 0;
    };

    // One table row as the forward program reads it.
    struct TextureData {
        // DescriptorHandle<Texture2D>
        u64 texture = 0;
        u32 sampler = 0;
        u32 _pad = 0;
    };
    static_assert(sizeof(TextureData) == 16);
    static_assert(std::is_trivially_copyable_v<TextureData>);
}
