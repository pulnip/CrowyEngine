#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "math.hpp"
#include "string.hpp"
#include "RHIAPI.hpp"

namespace Crowy
{
    using RenderType = std::string;
    using RenderTypeHash = std::invoke_result_t<std::hash<RenderType>, RenderType>;

    using CBufferFieldName = std::string;

    enum class CBufferFieldType{
        Unknown,
        Int32, Float,
        Float2, Float3, Float4, Float4x4
    };

    template<typename T>
    constexpr bool is_convertible_to(CBufferFieldType type){
        switch(type){
        case CBufferFieldType::Int32:    return std::is_convertible_v<int32_t, T>;
        case CBufferFieldType::Float:    return std::is_convertible_v<float, T>;
        case CBufferFieldType::Float2:   return std::is_convertible_v<Vec2, T>;
        case CBufferFieldType::Float3:   return std::is_convertible_v<Vec3, T>;
        case CBufferFieldType::Float4:   return std::is_convertible_v<Vec4, T>;
        case CBufferFieldType::Float4x4: return std::is_convertible_v<Mat4, T>;
        default:
            std::unreachable();
        }
    }

    inline constexpr size_t size_of(CBufferFieldType type){
        switch(type){
        case CBufferFieldType::Int32:    return 4;
        case CBufferFieldType::Float:    return 4;
        case CBufferFieldType::Float2:   return 8;
        case CBufferFieldType::Float3:   return 16;
        case CBufferFieldType::Float4:   return 16;
        case CBufferFieldType::Float4x4: return 64;
        default:
            std::unreachable();
        }
    }
    using CBufferFieldOffset = size_t;

    struct CBufferFieldMeta{
        CBufferFieldType type;
        CBufferFieldOffset offset = 0;
    };
    using CBufferMeta = std::unordered_map<
        CBufferFieldName, CBufferFieldMeta,
        StringHash, std::equal_to<>
    >;
}