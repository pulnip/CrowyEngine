#pragma once

#include <span>
#include "DOM.hpp"
#include "EnumUtil.hpp"
#include "Primitives.hpp"

#define DECLARE_TYPETRAITS(TYPE) \
    template<> \
    struct TypeTraits<TYPE>{ \
        static constexpr CStr name = #TYPE; \
        static bool deserialize(void*, const DOM::Value&); \
        static void serialize(const void*, DOM::Value&); \
    };

namespace Crowy
{
    struct TypeDesc;

    // empty by default, so a type without traits can still be
    // reflected property by property. see MakeTypeOps
    template<typename T>
    struct TypeTraits{};

    template<typename T>
        requires HasEnumTraits<T>
    struct TypeTraits<T>{
        static constexpr CStr name = EnumTraits<T>::name;
        static bool deserialize(void* data, const DOM::Value& value){
            if(auto v = value.asString()){
                if(auto parsed = enumFromName<T>(*v)){
                    *static_cast<T*>(data) = *parsed;
                    return true;
                }
            }
            // an unknown name keeps the value; the caller decides what that means
            return false;
        }
        static void serialize(const void* data, DOM::Value& out){
            // a value with no enumerator has no name; null says so
            const auto name = enumName(*static_cast<const T*>(data));
            out = name != nullptr ? DOM::Value(name) : DOM::Value();
        }
    };

    DECLARE_TYPETRAITS(bool)
    DECLARE_TYPETRAITS(i8)
    DECLARE_TYPETRAITS(i16)
    DECLARE_TYPETRAITS(i32)
    DECLARE_TYPETRAITS(i64)
    DECLARE_TYPETRAITS(u8)
    DECLARE_TYPETRAITS(u16)
    DECLARE_TYPETRAITS(u32)
    DECLARE_TYPETRAITS(u64)
    DECLARE_TYPETRAITS(f32)
    DECLARE_TYPETRAITS(f64)
    DECLARE_TYPETRAITS(Str)
    DECLARE_TYPETRAITS(Vec2)
    DECLARE_TYPETRAITS(Vec3)
    DECLARE_TYPETRAITS(Vec4)
    DECLARE_TYPETRAITS(Size2D)
    DECLARE_TYPETRAITS(Transform)

    template<typename T>
    concept HasTypeTraits = requires{
        TypeTraits<T>::name;
        TypeTraits<T>::deserialize;
        TypeTraits<T>::serialize;
    };

    struct EnumeratorDesc{
        CStr name;
        i64 value;
    };

    // erased operations and identity of one type.
    // built by MakeTypeOps, see ClassRegistry.hpp
    struct TypeOps{
        CStr name = nullptr;
        usize size = 0;
        // leaf type: parses the whole value at once.
        // false when the value does not bind, and the member is untouched
        bool (*deserialize)(void*, const DOM::Value&) = nullptr;
        // leaf type: emits the value deserialize reads back
        void (*serialize)(const void*, DOM::Value&) = nullptr;
        // reflected type: filled property by property.
        // resolved lazily, so the desc may register after this TypeOps was built
        const TypeDesc* (*getDesc)() = nullptr;
        // enum type: its enumerators, for dropdowns and by-name writers
        std::span<const EnumeratorDesc> (*enumerators)() = nullptr;
        // enum type: the member as the i64 enumerators() lists,
        // so an erased reader compares with no width or sign guess
        i64 (*enumLoad)(const void*) = nullptr;
        void (*enumStore)(void*, i64) = nullptr;
    };
}

#undef DECLARE_TYPETRAITS
