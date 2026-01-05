#include "EntityScript.hpp"

namespace Crowy
{
    EntityScript::~EntityScript(){
        for(auto& [_, monoScript]: monoScripts){
            monoScript.object->Release();
        }
    }

    void EntityScript::attach(std::string name, MonoScript monoScript){
        auto it = monoScripts.find(name);
        if(it != monoScripts.end())
            return;

        monoScripts.emplace(std::move(name), std::move(monoScript));
    }

    void EntityScript::detach(std::string_view name){
        auto it = monoScripts.find(name);
        if(it == monoScripts.end())
            return;

        monoScripts.erase(it);
    }

    MonoScript* EntityScript::get(std::string_view name){
        return const_cast<MonoScript*>(
            static_cast<const EntityScript&>(*this).get(name)
        );
    }

    const MonoScript* EntityScript::get(std::string_view name) const{
        auto it = monoScripts.find(name);
        if(it == monoScripts.end())
            return nullptr;

        return &it->second;
    }

    void EntityScript::start(asIScriptContext* ctx){
        for(auto& [_, monoScript]: monoScripts){
            if(!monoScript.onStart)
                continue;

            ctx->Prepare(monoScript.onStart);
            ctx->SetObject(monoScript.object);
            ctx->Execute();
        }
    }

    void EntityScript::update(asIScriptContext* ctx, float dt){
        for(auto& [_, monoScript]: monoScripts){
            if(!monoScript.onUpdate)
                continue;

            ctx->Prepare(monoScript.onUpdate);
            ctx->SetObject(monoScript.object);
            ctx->SetArgFloat(0, dt);
            ctx->Execute();
        }
    }

    void EntityScript::finish(asIScriptContext* ctx){
        for(auto& [_, monoScript]: monoScripts){
            if(!monoScript.onFinish)
                continue;

            ctx->Prepare(monoScript.onFinish);
            ctx->SetObject(monoScript.object);
            ctx->Execute();
        }
    }
}