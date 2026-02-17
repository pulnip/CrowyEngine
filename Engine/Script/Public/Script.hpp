#pragma once

#include "ResourceHandle.hpp"
#include "EntityHandle.hpp"

namespace Crowy
{
    struct ScriptSpec;
    struct ScriptInstanceSpec;
    struct EntityHandle;

    void initScriptModule();
    void deinitScriptModule();

    void loadScriptConfig(const ScriptSpec&);
    void unloadScriptConfig();

    ScriptHandle createScriptInstance(const ScriptInstanceSpec&, EntityHandle);
    void destroyScriptInstance(ScriptHandle);

    void startScript(ScriptHandle);
    void updateScript(ScriptHandle, float dt);
    void finishScript(ScriptHandle);

    void startAllScript();
    void updateAllScript(float dt);
    void finishAllScript();
}