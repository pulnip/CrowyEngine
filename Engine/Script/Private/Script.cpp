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
        ScriptRuntime_->load(spec);
    }

    void unloadScriptConfig(){
        ScriptRuntime_->unload();
    }

    ScriptHandle createScriptInstance(const ScriptInstanceSpec& spec, EntityHandle handle){
        return ScriptRuntime_->create(spec, handle);
    }

    void destroyScriptInstance(ScriptHandle handle){
        ScriptRuntime_->destroy(handle);
    }

    void startScript(ScriptHandle handle){
        ScriptRuntime_->start(handle);
    }

    void updateScript(ScriptHandle handle, float dt){
        ScriptRuntime_->update(handle, dt);
    }

    void finishScript(ScriptHandle handle){
        ScriptRuntime_->finish(handle);
    }

    void startAllScript(){
        ScriptRuntime_->startAll();
    }

    void updateAllScript(float dt){
        ScriptRuntime_->updateAll(dt);
    }

    void finishAllScript(){
        ScriptRuntime_->finishAll();
    }
}