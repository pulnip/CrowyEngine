#pragma once

#include <unordered_map>
#include <angelscript.h>
#include "slot_map.hpp"
#include "EntityScript.hpp"
#include "ResourceHandle.hpp"

namespace Crowy
{
    struct ScriptSpec;
    struct ScriptInstanceSpec;

    class ScriptRuntime{
    private:
        static ScriptRuntime* instance;
        friend void initScriptModule();
        friend void deinitScriptModule();

        asIScriptEngine* engine;
        std::unordered_map<std::string, asIScriptModule*> modules;
        std::unordered_map<std::string, asITypeInfo*> types;

        // single thread only, use context pool later.
        asIScriptContext* context;
        slot_map<EntityScript> scripts;

    public:
        ScriptRuntime();
        ~ScriptRuntime();

        inline static auto singleton(){ return instance; }

        void load(const ScriptSpec&);
        void unload();

        ScriptHandle create(const ScriptInstanceSpec&);
        void destroy(ScriptHandle);

        EntityScript* find(ScriptHandle);
        const EntityScript* find(ScriptHandle) const;

        void start(ScriptHandle);
        void update(ScriptHandle, float dt);
        void finish(ScriptHandle);

        void startAll();
        void updateAll(float dt);
        void finishAll();
    };
}