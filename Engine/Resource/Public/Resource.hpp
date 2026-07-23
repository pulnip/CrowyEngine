#pragma once

#include <filesystem>
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    template<typename T>
    concept ResourceRequest = requires(T t){
        typename T::Key;
        typename T::KeyHash;
    };

    template<typename T>
    concept Resource = requires{
        typename T::Request;
        requires ResourceRequest<typename T::Request>;
    };

    struct SpriteAnimation{
        u32 startRow = 0;
        u32 startCol, frameCount = 1;
        f32 frameDurationMs = 160.0f;
    };

    struct SpriteRequest{
        using Key = std::filesystem::path;
        using KeyHash = std::hash<Key>;

        Key path;
        Size2D sheetSize;
        StringHashMap<SpriteAnimation> animations;
    };

    struct SpriteResource{
        using Request = SpriteRequest;

        RHITextureRAII texture;
        Size2D sheetSize;
        StringHashMap<SpriteAnimation> animations;
    };
    static_assert(Resource<SpriteResource>);
}
