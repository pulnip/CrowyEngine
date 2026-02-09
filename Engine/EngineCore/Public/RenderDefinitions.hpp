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

namespace Crowy
{
    using RenderType = std::string;
    using RenderTypeHash = std::invoke_result_t<std::hash<RenderType>, RenderType>;

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

    class CBuffer{
    private:
        using FieldName = std::string;
        using FieldType = CBufferFieldType;
        using FieldOffset = size_t;
        struct FieldMeta{
            FieldType type;
            FieldOffset offset = 0;
        };

        std::unordered_map<FieldName, FieldMeta, StringHash, std::equal_to<>> meta;
        std::vector<std::byte> payload;

        struct ConstFieldProxy{
            const FieldType type;
            const std::byte& ref;

            template<typename T>
            operator T() const{
                T t;
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&t, &ref, size_of(type));

                return t;
            }
        };

        struct FieldProxy{
            const FieldType type;
            std::byte& ref;

            template<typename T>
            FieldProxy& operator=(const T& t){
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&ref, &t, size_of(type));
                return *this;
            }

            template<typename T>
            operator T() const{
                T t;
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&t, &ref, size_of(type));

                return t;
            }
        };

    public:
        inline auto newField(
            std::string_view name,
            FieldType type
        ){
            auto [it, succeed] = meta.try_emplace(std::string(name), FieldMeta{
                .type = type,
                .offset = payload.size()
            });

            if(succeed)
                payload.resize(payload.size() + size_of(type));

            auto& m = it->second;
            return FieldProxy{
                .type = m.type,
                .ref = payload[m.offset]
            };
        }

        std::optional<ConstFieldProxy> at(std::string_view name) const{
            auto it = meta.find(name);
            if(it == meta.end())
                return std::nullopt;

            auto& m = it->second;
            return ConstFieldProxy{
                .type = m.type,
                .ref = payload[m.offset]
            };
        }
        std::optional<FieldProxy> at(std::string_view name){
            auto it = meta.find(name);
            if(it == meta.end())
                return std::nullopt;

            auto& m = it->second;
            return FieldProxy{
                .type = m.type,
                .ref = payload[m.offset]
            };
        }

        inline void* data() noexcept{ return payload.data(); }
        inline auto size() const noexcept{ return payload.size(); }
    };
}