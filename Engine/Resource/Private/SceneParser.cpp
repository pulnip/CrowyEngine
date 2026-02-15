#include <toml++/toml.hpp>
#include "path_util.hpp"
#include "ConfigParser.hpp"
#include "InternalComponentBinder.hpp"
#include "ParserCommon.hpp"
#include "SceneParserPrivate.hpp"

namespace Crowy
{
    SceneSpec buildScene(const ParseResult& temp, const ComponentBinderRegistry& registry){
        SceneSpec out;
        ComponentBindPlan plan;

        // reserve entity slot and copy name.
        out.entities.resize(temp.elements.size());
        for(size_t i=0; i<temp.elements.size(); ++i){
            const auto& elm = temp.elements[i];
            const auto& node = temp.arena.nodes[elm.index];

            if(auto table = std::get_if<VTable>(&node)){
                auto name = readString(temp.arena, *table, plan.errors, "name");

                if(name.has_value())
                    out.entities[i].name = *name;
            }
            else{
                // TODO. write Error
            }                                                                  
        }

        bindAndErrorReport(temp, registry, plan);
        reportError(plan.errors);

        // Freeze(Create SoA + connect index)
        TransformBinder::freeze(out, plan);
        RenderObjectBinder::freeze(out, plan);
        RigidbodyBinder::freeze(out, plan);
        BoxColliderBinder::freeze(out, plan);
        SphereColliderBinder::freeze(out, plan);
        CameraBinder::freeze(out, plan);
        PlayerBinder::freeze(out, plan);
        EditorBinder::freeze(out, plan);
        // Other ComponentBinder::freeze...

        return out;
    }

    SceneSpec parseCustomSceneFromFile(const std::filesystem::path& sceneFile,
        const ComponentBinderRegistry& binderRegistry
    ){
        auto u8strPath = to_utf8String(sceneFile);
        toml::parse_result pr = toml::parse_file(u8strPath);
        if(pr.empty())
            return {};

        auto tempScene = parseFromTable(*pr.as_table(), "entities");
        return buildScene(tempScene, binderRegistry);
    }

    SceneSpec parseCustomSceneFromString(std::string_view tomlText,
        const ComponentBinderRegistry& binderRegistry
    ){
        toml::parse_result pr = toml::parse(tomlText);
        if(pr.empty())
            return {};

        auto tempScene = parseFromTable(*pr.as_table(), "entities");
        return buildScene(tempScene, binderRegistry);
    }

    SceneSpec parseSceneFromFile(const std::filesystem::path& sceneFile){
        auto defaultBinderRegistry = makeDefaultComponentBinderRegistry();

        return parseCustomSceneFromFile(sceneFile, defaultBinderRegistry);
    }

    SceneSpec parseSceneFromString(std::string_view sceneText){
        auto defaultBinderRegistry = makeDefaultComponentBinderRegistry();

        return parseCustomSceneFromString(sceneText, defaultBinderRegistry);
    }
}