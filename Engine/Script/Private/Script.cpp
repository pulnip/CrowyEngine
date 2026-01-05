#include "Script.hpp"
#include "ScriptRuntime.hpp"

namespace Crowy
{
    ScriptRuntime* ScriptRuntime::instance = nullptr;

    void initScriptModule(){
        ScriptRuntime::instance = new ScriptRuntime();
    }

    void deinitScriptModule(){
        delete ScriptRuntime::instance;
        ScriptRuntime::instance = nullptr;
    }

    void loadScriptConfig(const ScriptSpec& spec){
        ScriptRuntime::singleton()->load(spec);
    }

    void unloadScriptConfig(){
        ScriptRuntime::singleton()->unload();
    }

    ScriptHandle createScriptInstance(const ScriptInstanceSpec& spec){
        return ScriptRuntime::singleton()->create(spec);
    }

    void destroyScriptInstance(ScriptHandle handle){
        ScriptRuntime::singleton()->destroy(handle);
    }

    void startScript(ScriptHandle handle){
        ScriptRuntime::singleton()->start(handle);
    }

    void updateScript(ScriptHandle handle, float dt){
        ScriptRuntime::singleton()->update(handle, dt);
    }

    void finishScript(ScriptHandle handle){
        ScriptRuntime::singleton()->finish(handle);
    }

    void startAllScript(){
        ScriptRuntime::singleton()->startAll();
    }

    void updateAllScript(float dt){
        ScriptRuntime::singleton()->updateAll(dt);
    }

    void finishAllScript(){
        ScriptRuntime::singleton()->finishAll();
    }
}