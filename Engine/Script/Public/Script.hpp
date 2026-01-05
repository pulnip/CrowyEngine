#pragma once

#include "ResourceHandle.hpp"

namespace Crowy
{
    struct ScriptSpec;
    struct ScriptInstanceSpec;

    void initScriptModule();
    void deinitScriptModule();

    void loadScriptConfig(const ScriptSpec&);
    void unloadScriptConfig();

    ScriptHandle createScriptInstance(const ScriptInstanceSpec&);
    void destroyScriptInstance(ScriptHandle);

    void startScript(ScriptHandle);
    void updateScript(ScriptHandle, float dt);
    void finishScript(ScriptHandle);

    void startAllScript();
    void updateAllScript(float dt);
    void finishAllScript();
}