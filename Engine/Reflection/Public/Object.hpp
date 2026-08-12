#pragma once

#include "ClassRegistry.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class Object{
    public:
        Object() = default;
        virtual ~Object() = default;
        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(Object)

        static constexpr CStr StaticClassName(){
            return "Object";
        }
        virtual CStr GetClassName() const{
            return StaticClassName();
        }

        virtual bool IsA(StrView name) const{
            return name == StaticClassName();
        }
        static auto _CrowyReflectImpl();
    };

    template<typename T>
        requires std::is_base_of_v<Object, T>
    ClassBuilder<T> Reflect(){
        return ClassBuilder<T>();
    }
}

#define CROWY_MACRO_DISPATCHER_FOR_2_ARGS(_1, _2, NAME, ...) NAME

// Object Body macro
#define CROWY_OBJECT_BODY_IMPL(TYPE, PARENT) \
public: \
    static constexpr ::Crowy::CStr StaticClassName(){ \
        return #TYPE; \
    } \
    virtual ::Crowy::CStr GetClassName() const override{ \
        return StaticClassName(); \
    } \
    using Super = PARENT; \
    virtual bool IsA(::Crowy::StrView name) const override{ \
        if(name == StaticClassName()) \
            return true; \
        return Super::IsA(name); \
    } \
    static auto _CrowyReflectImpl(); \
private:

#define CROWY_OBJECT_BODY_DIRECT(TYPE) \
    CROWY_OBJECT_BODY_IMPL(TYPE, ::Crowy::Object)

#define CROWY_OBJECT_BODY_HELPER(_1, _2, NAME, ...) NAME
#define CROWY_OBJECT_BODY(...) \
    CROWY_OBJECT_BODY_HELPER( \
        __VA_ARGS__, \
        CROWY_OBJECT_BODY_IMPL, \
        CROWY_OBJECT_BODY_DIRECT \
    )(__VA_ARGS__)

// Object Registration start macro
#define CROWY_OBJECT_IMPL(TYPE, PARENT) \
auto TYPE::_CrowyReflectImpl(){ \
    return ::Crowy::Reflect<TYPE>() \
        .SetName(#TYPE) \
        .Inherits<PARENT>()

#define CROWY_OBJECT_DIRECT(TYPE) \
    CROWY_OBJECT_IMPL(TYPE, ::Crowy::Object)

#define CROWY_OBJECT(...) \
    CROWY_MACRO_DISPATCHER_FOR_2_ARGS( \
        __VA_ARGS__, \
        CROWY_OBJECT_IMPL, \
        CROWY_OBJECT_DIRECT \
    )(__VA_ARGS__)

// Object Registration end macro
#define CROWY_OBJECT_END(TYPE) \
        .Build(); \
} \
namespace{ \
    const auto Is##TYPE##Registered = TYPE::_CrowyReflectImpl(); \
}
