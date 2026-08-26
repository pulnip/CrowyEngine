#include "TypeOps.hpp"

#define DEFINE_TYPETRAITS(TYPE) \
    void TypeTraits<TYPE>::deserialize(void* data, const DOM::Value& value){ \
        if(auto v = value.get<TYPE>()){ \
            *static_cast<TYPE*>(data) = *v; \
        } \
    }

namespace Crowy
{
    DEFINE_TYPETRAITS(bool)
    DEFINE_TYPETRAITS(i8)
    DEFINE_TYPETRAITS(i16)
    DEFINE_TYPETRAITS(i32)
    DEFINE_TYPETRAITS(i64)
    DEFINE_TYPETRAITS(u8)
    DEFINE_TYPETRAITS(u16)
    DEFINE_TYPETRAITS(u32)
    DEFINE_TYPETRAITS(u64)
    DEFINE_TYPETRAITS(f32)
    DEFINE_TYPETRAITS(f64)
    DEFINE_TYPETRAITS(Str)
    DEFINE_TYPETRAITS(Vec2)
    DEFINE_TYPETRAITS(Vec3)
    DEFINE_TYPETRAITS(Vec4)
    DEFINE_TYPETRAITS(Size2D)
    DEFINE_TYPETRAITS(Transform)
}
