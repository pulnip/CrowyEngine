#pragma once

#include <span>
#include "DOM.hpp"
#include "EnumUtil.hpp"
#include "Primitives.hpp"

#define DECLARE_TYPETRAITS(TYPE) \
    template<> \
    struct TypeTraits<TYPE>{ \
        static constexpr CStr name = #TYPE; \
        static void deserialize(void*, const DOM::Value&); \
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
        static void deserialize(void* data, const DOM::Value& value){
            if(auto v = value.asString()){
                // an unknown name keeps the default, like an absent key
                if(auto parsed = enumFromName<T>(*v)){
                    *static_cast<T*>(data) = *parsed;
                }
            }
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
        // leaf type: parses the whole value at once
        void (*deserialize)(void*, const DOM::Value&) = nullptr;
        // reflected type: filled property by property.
        // resolved lazily, so the desc may register after this TypeOps was built
        const TypeDesc* (*getDesc)() = nullptr;
        // enum type: its enumerators, for dropdowns and by-name writers
        std::span<const EnumeratorDesc> (*enumerators)() = nullptr;
    };
}

#undef DECLARE_TYPETRAITS
