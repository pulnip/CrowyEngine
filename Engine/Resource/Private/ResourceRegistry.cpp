#include "DOM.hpp"
#include "LogLocal.hpp"
#include "ResourceRegistry.hpp"
#include "RHITexture.hpp"
#include "StringUtil.hpp"
#include "TomlLoader.hpp"

namespace{
    Crowy::StringHashMap<Crowy::SpriteAnimation> gatherAnimations(const Crowy::DOM::Value& node){
        using namespace Crowy;

        StringHashMap<SpriteAnimation> animations;

        node.forEach("animations", [&animations](const DOM::Value& node){
            auto name = node.get<Str>("name");
            if(!name.has_value()){
                LOG_ERROR("no \"name\" in [[animations]]");
                return;
            }

            auto startRow = node.get<u32>("start_row")
                .value_or(0);
            auto startCol = node.get<u32>("start_col")
                .value_or(0);
            auto frameCount = node.get<u32>("frame_count")
                .value_or(1);
            auto frameDurationMs = node.get<f32>("frame_duration_ms")
                .value_or(150);

            animations.try_emplace(*name, SpriteAnimation{
                .startRow = startRow,
                .startCol = startCol,
                .frameCount = frameCount,
                .frameDurationMs = frameDurationMs
            });
        });

        return animations;
    }

    struct SpriteConfig{
        Crowy::TomlMetadata metadata;
        Crowy::SpriteRequest data;
    };

    Crowy::SpriteRequest createSpriteRequest(
        const Crowy::DOM::Value& dom,
        const std::filesystem::path& contentRoot
    ){
        using namespace Crowy;

        // use sprite meta file
        auto sprite = dom.get<Str>("file");
        if(sprite.has_value()){
            auto path = contentRoot / *sprite;
            auto config = loadTomlFile<SpriteConfig>(path);

            return config.data;
        }

        // fallback, use just image
        auto imagePath = dom.get<Str>("image_path");
        if(imagePath.has_value()){
            return SpriteRequest{
                .path = *imagePath,
                .sheetSize = {1, 1}
            };
        }

        LOG_ERROR("key \"file\" or \"image_path\" not found");
        return SpriteRequest{};
    }
}

namespace Crowy
{
    template<>
    struct TomlTraits<SpriteConfig>{
        static SpriteConfig from(const DOM::Value& dom, const TomlMetadata& metadata){
            auto image = dom.get<Str>("sheet.image")
                .value_or({});
            auto rows = dom.get<u32>("sheet.rows")
                .value_or(1);
            auto columns = dom.get<u32>("sheet.columns")
                .value_or(1);

            return SpriteConfig{
                .metadata = metadata,
                .data =  SpriteRequest{
                    .path = image,
                    .sheetSize = {columns, rows},
                    .animations = gatherAnimations(dom)
                }
            };
        }
    };

    ResourceRegistry::ResourceRegistry(
        const DOM::Value& manifest,
        const std::filesystem::path& contentRoot,
        RAII<SpriteLoader>&& spriteLoader
    )
        : spriteLoader(std::move(spriteLoader))
        , spriteManager(*(this->spriteLoader))
    {
        auto metadata = TomlTraits<TomlMetadata>::from(manifest);
        CROWY_ASSERT(metadata.type == "resources");

        manifest.forEach("sprites", [&](const DOM::Value& node){
            auto name = node.get<Str>("name");
            if(!name.has_value()){
                LOG_ERROR("No resource identifier");
                return;
            }

            auto file = node.get<Str>("file");
            if(!file.has_value()){
                LOG_ERROR("No resource path in: {}", *name);
                return;
            }

            auto request = createSpriteRequest(node, contentRoot);
            spriteManager.Load(*name, request);
        });
    }

    void ResourceRegistry::DrainAllCompletions(){
        spriteManager.DrainCompletions();
    }
}
