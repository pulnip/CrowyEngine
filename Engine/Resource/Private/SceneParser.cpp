#include <toml++/toml.hpp>
#include "InternalComponentBinder.hpp"
#include "ParserCommon.hpp"
#include "SceneParser.hpp"
#include "SceneParserPrivate.hpp"

namespace Crowy
{
    SceneSpec buildScene(const ParseResult& temp, const ComponentBinderRegistry& registry){
        SceneSpec out;
        // reserve entity slot and copy name.
        out.entities.resize(temp.elements.size());
        for(size_t i=0; i<temp.elements.size(); ++i){
            out.entities[i].name = temp.elements[i].name;
        }

        auto plan = bindAndErrorReport(temp, registry);

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

    SceneSpec parseCustomSceneFromFile(std::string_view sceneFile,
        const ComponentBinderRegistry& binderRegistry
    ){
        toml::parse_result pr = toml::parse_file(sceneFile);
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

    SceneSpec parseSceneFromFile(std::string_view sceneFile){
        auto defaultBinderRegistry = makeDefaultComponentBinderRegistry();

        return parseCustomSceneFromFile(sceneFile, defaultBinderRegistry);
    }

    SceneSpec parseSceneFromString(std::string_view sceneText){
        auto defaultBinderRegistry = makeDefaultComponentBinderRegistry();

        return parseCustomSceneFromString(sceneText, defaultBinderRegistry);
    }
}