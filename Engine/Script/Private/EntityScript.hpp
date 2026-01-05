#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <angelscript.h>
#include "semantics.hpp"
#include "string.hpp"

namespace Crowy
{
    struct MonoScript{
        asIScriptObject* object;
        // cache
        asIScriptFunction* onStart;
        asIScriptFunction* onUpdate;
        asIScriptFunction* onFinish;
    };

    class EntityScript{
    private:
        std::unordered_map<std::string, MonoScript, StringHash, std::equal_to<>> monoScripts;

    public:
        EntityScript() = default;
        ~EntityScript();
        DECLARE_NON_COPYABLE(EntityScript)
        EntityScript(EntityScript&&);
        EntityScript& operator=(EntityScript&&);

        void attach(std::string name, MonoScript);
        void detach(std::string_view name);

        MonoScript* get(std::string_view name);
        const MonoScript* get(std::string_view name) const;

        void start(asIScriptContext*);
        void update(asIScriptContext*, float dt);
        void finish(asIScriptContext*);
    };
}