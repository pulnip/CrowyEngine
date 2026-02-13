#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "math.hpp"
#include "string.hpp"

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

    struct CBufferConstFieldProxy{
        const CBufferFieldType type;
        const std::byte& ref;

        template<typename T>
        operator T() const{
            T t;
            if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                std::memcpy(&t, &ref, size_of(type));

            return t;
        }
    };

    struct CBufferFieldProxy{
        const CBufferFieldType type;
        std::byte& ref;

        template<typename T>
        CBufferFieldProxy& operator=(const T& t){
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

    struct CBuffer{
        using FieldName = CBufferFieldName;
        using FieldType = CBufferFieldType;
        using FieldOffset = CBufferFieldOffset;
        using FieldMeta = CBufferFieldMeta;
        using ConstFieldProxy = CBufferConstFieldProxy;
        using FieldProxy = CBufferFieldProxy;

        struct Field{
            FieldName name;
            FieldMeta meta;
        };
        using ConstFieldView = std::pair<
            std::string_view, ConstFieldProxy
        >;
        using FieldView = std::pair<
            std::string_view, FieldProxy
        >;

        std::string name;
        uint32_t slot;
        std::vector<Field> fields;
        std::unordered_map<
            FieldName, size_t,
            StringHash, std::equal_to<>
        > fieldIndex;
        std::vector<std::byte> buffer;

        inline auto newField(
            std::string_view name,
            FieldType type
        ){
            auto [it, succeed] = fieldIndex.try_emplace(std::string(name), fields.size());
            if(succeed){
                fields.push_back({
                    .name = std::string(name),
                    .meta = {
                        .type = type,
                        .offset = buffer.size()
                    }
                });
                buffer.resize(buffer.size() + size_of(type));
            }

            auto& field = fields[it->second];
            return FieldProxy{
                .type = field.meta.type,
                .ref = buffer[field.meta.offset]
            };
        }

        inline std::optional<ConstFieldProxy> at(std::string_view name) const{
            auto it = fieldIndex.find(name);
            if(it == fieldIndex.end())
                return std::nullopt;

            auto& field = fields[it->second];
            return ConstFieldProxy{
                .type = field.meta.type,
                .ref = buffer[field.meta.offset]
            };
        }

        inline std::optional<FieldProxy> at(std::string_view name){
            auto it = fieldIndex.find(name);
            if(it == fieldIndex.end())
                return std::nullopt;

            auto& field = fields[it->second];
            return FieldProxy{
                .type = field.meta.type,
                .ref = buffer[field.meta.offset]
            };
        }

        inline auto fieldViews() const{
            return fields | std::views::transform(
                [this](const auto& field){
                    return ConstFieldView{
                        field.name, {
                        .type = field.meta.type,
                        .ref = buffer[field.meta.offset]
                    }};
                }
            );
        }

        inline auto fieldViews(){
            return fields | std::views::transform(
                [this](auto& field){
                    return FieldView{
                        field.name, {
                        .type = field.meta.type,
                        .ref = buffer[field.meta.offset]
                    }};
                }
            );
        }
    };
}