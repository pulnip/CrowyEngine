#pragma once

#include "Resource.hpp"
#include "ResourceManager.hpp"
#include "ResourceLoader.hpp"

namespace Crowy
{
    namespace DOM
    {
        class Value;
    }

    class ResourceRegistry{
    private:
        using SpriteLoader = ResourceLoader<SpriteResource>;
        using SpriteManager = ResourceManager<SpriteResource>;

        RAII<SpriteLoader> spriteLoader;
        SpriteManager spriteManager;

    public:
        ResourceRegistry(
            const DOM::Value& manifest,
            const std::filesystem::path& contentRoot,
            RAII<ResourceLoader<SpriteResource>>&&
        );

        void DrainAllCompletions();

        SpriteManager& GetSpriteManager() noexcept{
            return spriteManager;
        }
    };
}
