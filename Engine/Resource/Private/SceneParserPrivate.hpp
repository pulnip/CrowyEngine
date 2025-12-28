#pragma once

#include <string_view>
#include "SceneSpec.hpp"
#include "InternalComponentBinder.hpp"

namespace Crowy
{
    /**
     * Parsing Pipeline:
     * 
     * TOML text
     *     ↓ parseSceneFromTable()
     * TempScene (toml++ Dependency Isolation)
     *     ↓ validateAndPlan() 
     * BindPlan (collect all error, and report once)
     *     ↓ freeze()
     * SceneSpec
     */
    SceneSpec parseCustomSceneFromFile(std::string_view sceneFile,
        const ComponentBinderRegistry& binderRegistry
    );
    SceneSpec parseCustomSceneFromString(std::string_view sceneText,
        const ComponentBinderRegistry& binderRegistry
    );
}