#include <cassert>
#include <print>
#include <scriptarray.h>
#include <scriptmath.h>
#include <scriptstdstring.h>
#include "string.hpp"
#include "Log.hpp"
#include "ScriptRuntime.hpp"
#include "ScriptSpec.hpp"

using Crowy::LOG_SCRIPT;

static void MessageCallback(const asSMessageInfo* msg, void* param){
    switch(msg->type){
    case asMSGTYPE_ERROR:
        LOG_ERROR(LOG_SCRIPT, "{} ({}:{}:{})",
            msg->message, msg->section, msg->row, msg->col
        );
        break;
    case asMSGTYPE_WARNING:
        LOG_WARN(LOG_SCRIPT, "{} ({}:{}:{})",
            msg->message, msg->section, msg->row, msg->col
        );
        break;
    case asMSGTYPE_INFORMATION:
        LOG_INFO(LOG_SCRIPT, "{} ({}:{}:{})",
            msg->message, msg->section, msg->row, msg->col
        );
        break;
    }
}

static void println(const std::string& msg){
    std::println("{}", msg);
}

namespace Crowy
{
    ScriptRuntime::ScriptRuntime()
        : engine(asCreateScriptEngine())
        , context(engine->CreateContext())
    {
        auto r = engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
        assert(r >= 0);

        RegisterScriptArray(engine, true);
        RegisterScriptMath(engine);
        RegisterStdString(engine);

        r = engine->RegisterGlobalFunction("void println(const string &in)", asFUNCTION(println), asCALL_CDECL);
        assert(r >= 0);
    }

    ScriptRuntime::~ScriptRuntime(){
        scripts.clear();
        context->Release();
        engine->ShutDownAndRelease();
    }

    void ScriptRuntime::load(const ScriptSpec& spec){
        for(const auto& moduleSpec: spec.modules){
            auto mod = engine->GetModule(moduleSpec.name.c_str(), asGM_ALWAYS_CREATE);

            for(const auto& file: moduleSpec.files){
                auto code = readFileAsString(file);
                mod->AddScriptSection(file.c_str(), code.c_str(), code.size());
            }

            mod->Build();
            modules.emplace(moduleSpec.name, mod);

            for(asUINT i=0; i<mod->GetObjectTypeCount(); ++i){
                auto type = mod->GetObjectTypeByIndex(i);
                types.emplace(type->GetName(), type);
            }
        }
    }


    ScriptHandle ScriptRuntime::create(const ScriptInstanceSpec& spec){
        EntityScript entityScript;

        for(const auto& monoScript: spec.monoScripts){
            auto it = types.find(monoScript);
            if(it == types.end())
                continue;

            auto type = it->second;
            auto factory = type->GetFactoryByIndex(0);
            context->Prepare(factory);
            context->Execute();

            auto obj = *(asIScriptObject**)context->GetAddressOfReturnValue();
            obj->AddRef();

            entityScript.attach(monoScript, MonoScript{
                .object = obj,
                .onStart  = type->GetMethodByName("onStart"),
                .onUpdate = type->GetMethodByName("onUpdate"),
                .onFinish = type->GetMethodByName("onFinish")
            });
        }

        return scripts.emplace(std::move(entityScript));
    }

    void ScriptRuntime::destroy(ScriptHandle handle){
        scripts.remove(handle);
    }

    EntityScript* ScriptRuntime::find(ScriptHandle handle){
        return const_cast<EntityScript*>(
            static_cast<const ScriptRuntime&>(*this).find(handle)
        );
    }

    const EntityScript* ScriptRuntime::find(ScriptHandle handle) const{
        return scripts.find(handle);
    }

    void ScriptRuntime::startAll(){
        for(auto& script: scripts){
            script.start(context);
        }
    }

    void ScriptRuntime::updateAll(float dt){
        for(auto& script: scripts){
            script.update(context, dt);
        }
    }

    void ScriptRuntime::finishAll(){
        for(auto& script: scripts){
            script.finish(context);
        }
    }
}