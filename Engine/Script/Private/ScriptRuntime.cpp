#include <cassert>
#include <format>
#include <print>
#include <stdexcept>
#include <angelscript.h>
#include <scriptarray.h>
#include <scriptmath.h>
#include <scriptstdstring.h>
#include "ComponentDefinitions.hpp"
#include "path_util.hpp"
#include "string.hpp"
#include "EntityHandle.hpp"
#include "Input.hpp"
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
    #define VALUE_OBJ(obj) \
        engine->RegisterObjectType(#obj, sizeof(obj), \
            asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<obj>())
    #define NO_RC_OBJ(obj) \
        engine->RegisterObjectType(#obj, 0, \
            asOBJ_REF | asOBJ_NOCOUNT);
    #define OBJ_PROP(obj, type, name) \
        engine->RegisterObjectProperty(#obj, #type" "#name, asOFFSET(obj, name))
    #define OBJ_MEMBER_FUNC(obj, func, sign) \
        engine->RegisterObjectMethod(#obj, sign, \
            asMETHOD(obj, func), asCALL_THISCALL);
    #define OBJ_MEMBER_FUNC_HELPER(obj, func, sign) \
        engine->RegisterObjectMethod(#obj, sign, \
            asFUNCTION(func), asCALL_CDECL_OBJFIRST);

    static void registerPrimitive(asIScriptEngine* engine){
        VALUE_OBJ(Vec2);
        OBJ_PROP(Vec2, float, x);
        OBJ_PROP(Vec2, float, y);

        VALUE_OBJ(Vec3);
        OBJ_PROP(Vec3, float, x);
        OBJ_PROP(Vec3, float, y);
        OBJ_PROP(Vec3, float, z);

        VALUE_OBJ(Vec4);
        OBJ_PROP(Vec4, float, x);
        OBJ_PROP(Vec4, float, y);
        OBJ_PROP(Vec4, float, z);
        OBJ_PROP(Vec4, float, w);
    }

    static auto registerECS(asIScriptEngine* engine){
        NO_RC_OBJ(TransformComponent);
        OBJ_PROP(TransformComponent, Vec3, position);
        OBJ_PROP(TransformComponent, Vec4, rotation);
        OBJ_PROP(TransformComponent, Vec3, scale);

        NO_RC_OBJ(CharacterController);
        OBJ_PROP(CharacterController, Vec3, pendingDelta);

        VALUE_OBJ(EntityHandle);
        OBJ_MEMBER_FUNC(EntityHandle, getTransformComponent,
            "TransformComponent@ getTransformComponent()");
        OBJ_MEMBER_FUNC(EntityHandle, getCharacterController,
            "CharacterController@ getCharacterController()");
    }

    #undef OBJ_PROP
    #undef NO_RC_OBJ
    #undef VALUE_OBJ

    static auto isActionHelper(const std::string& action){
        return isAction(action);
    }

    static void registerInput(asIScriptEngine* engine){
        // register KeyCode
        engine->RegisterEnum("KeyCode");

    #define KEYCODE(name) \
        engine->RegisterEnumValue("KeyCode", #name, static_cast<int>(KeyCode::name))

        // Numbers
        KEYCODE(Num0); KEYCODE(Num1); KEYCODE(Num2); KEYCODE(Num3); KEYCODE(Num4);
        KEYCODE(Num5); KEYCODE(Num6); KEYCODE(Num7); KEYCODE(Num8); KEYCODE(Num9);

        // Letters
        KEYCODE(A); KEYCODE(B); KEYCODE(C); KEYCODE(D); KEYCODE(E);
        KEYCODE(F); KEYCODE(G); KEYCODE(H); KEYCODE(I); KEYCODE(J);
        KEYCODE(K); KEYCODE(L); KEYCODE(M); KEYCODE(N); KEYCODE(O);
        KEYCODE(P); KEYCODE(Q); KEYCODE(R); KEYCODE(S); KEYCODE(T);
        KEYCODE(U); KEYCODE(V); KEYCODE(W); KEYCODE(X); KEYCODE(Y);
        KEYCODE(Z);

        // Function Keys
        KEYCODE(F1);  KEYCODE(F2);  KEYCODE(F3);
        KEYCODE(F4);  KEYCODE(F5);  KEYCODE(F6);
        KEYCODE(F7);  KEYCODE(F8);  KEYCODE(F9);
        KEYCODE(F10); KEYCODE(F11); KEYCODE(F12);

        // Modifiers
        KEYCODE(Ctrl); KEYCODE(Alt); KEYCODE(Shift);

        // Special
        KEYCODE(Tab); KEYCODE(Space); KEYCODE(Enter); KEYCODE(Escape);

        // Sentinel
        KEYCODE(Unknown);
    #undef KEYCODE

        // register key test code
        engine->RegisterGlobalFunction("bool isDown(KeyCode)", asFUNCTION(isDown), asCALL_CDECL);
        engine->RegisterGlobalFunction("bool isNone(KeyCode)", asFUNCTION(isNone), asCALL_CDECL);
        engine->RegisterGlobalFunction("bool isPressed(KeyCode)", asFUNCTION(isPressed), asCALL_CDECL);
        engine->RegisterGlobalFunction("bool isReleased(KeyCode)", asFUNCTION(isReleased), asCALL_CDECL);
        engine->RegisterGlobalFunction("bool isHeld(KeyCode)", asFUNCTION(isHeld), asCALL_CDECL);

        engine->RegisterGlobalFunction("bool isAction(const string &in)", asFUNCTION(isActionHelper), asCALL_CDECL);
    }

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

        registerPrimitive(engine);
        registerECS(engine);
        registerInput(engine);
    }

    ScriptRuntime::~ScriptRuntime(){
        unload();
        context->Release();
        engine->ShutDownAndRelease();
    }

    void ScriptRuntime::load(const ScriptSpec& spec){
        for(const auto& moduleSpec: spec.modules){
            auto mod = engine->GetModule(moduleSpec.name.c_str(), asGM_ALWAYS_CREATE);

            for(const auto& file: moduleSpec.files){
                auto resolvedPath = get_absolute_path(file);
                auto code = readFileAsString(resolvedPath);
                mod->AddScriptSection(moduleSpec.name.c_str(), code.c_str(), code.size());
            }

            mod->Build();
            modules.emplace(moduleSpec.name, mod);

            for(asUINT i=0; i<mod->GetObjectTypeCount(); ++i){
                auto type = mod->GetObjectTypeByIndex(i);
                types.emplace(type->GetName(), type);
            }
        }
    }

    void ScriptRuntime::unload(){
        scripts.clear();
        types.clear();
        modules.clear();
    }

    ScriptHandle ScriptRuntime::create(const ScriptInstanceSpec& spec, EntityHandle handle){
        EntityScript entityScript;

        for(const auto& monoScript: spec.monoScripts){
            auto it = types.find(monoScript);
            if(it == types.end())
                throw std::runtime_error(std::format(
                    "Type {} Not Found", monoScript
                ));

            auto type = it->second;
            auto factory = type->GetFactoryByIndex(0);
            context->Prepare(factory);
            context->SetArgObject(0, &handle);
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

    void ScriptRuntime::start(ScriptHandle handle){
        auto script = find(handle);
        if(!script)
            return;

        script->start(context);
    }

    void ScriptRuntime::update(ScriptHandle handle, float dt){
        auto script = find(handle);
        if(!script)
            return;

        script->update(context, dt);
    }

    void ScriptRuntime::finish(ScriptHandle handle){
        auto script = find(handle);
        if(!script)
            return;

        script->finish(context);
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