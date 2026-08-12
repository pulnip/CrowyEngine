#pragma once

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
    template<typename T>
    struct TypeTraits;

    template<typename T>
        requires std::is_enum_v<T>
    struct TypeTraits<T>{
        inline static CStr name = EnumTraits<T>::name;
        static void deserialize(void* data, const DOM::Value& value){
            if(auto v = value.asString()){
                *static_cast<T*>(data) = EnumTraits<T>::convert(*v);
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

    // use TypeInfo for type erasure
    struct TypeInfo{
        CStr name;
        usize size;
        void (*deserialize)(void*, const DOM::Value&);
    };

    template<typename T>
    const TypeInfo* GetTypeInfo(){
        static const TypeInfo info{
            .name = TypeTraits<T>::name,
            .size = sizeof(T),
            .deserialize = &TypeTraits<T>::deserialize
        };
        return &info;
    }
}

#undef DECLARE_TYPETRAITS
