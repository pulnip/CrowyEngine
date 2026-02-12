#pragma once

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "string.hpp"
#include "RenderDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct CBuffer{
        using FieldName = CBufferFieldName;
        using FieldType = CBufferFieldType;
        using FieldOffset = CBufferFieldOffset;
        using FieldMeta = CBufferFieldMeta;

        struct Field{
            FieldName name;
            FieldMeta meta;
        };

        std::string name;
        uint32_t slot;
        std::vector<Field> fields;
        std::unordered_map<FieldName, size_t, StringHash, std::equal_to<>> fieldIndex;
        RHIBufferPtr buffer;

        struct ConstFieldProxy{
            const FieldType type;
            const void* ptr;

            template<typename T>
            operator T() const{
                T t;
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&t, ptr, size_of(type));

                return t;
            }
        };

        struct FieldProxy{
            const FieldType type;
            void* ptr;

            template<typename T>
            FieldProxy& operator=(const T& t){
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(ptr, &t, size_of(type));
                return *this;
            }

            template<typename T>
            operator T() const{
                T t;
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&t, ptr, size_of(type));

                return t;
            }
        };

        struct ConstFieldView{
            std::string_view name;
            ConstFieldProxy field;
        };

        struct FieldView{
            std::string_view name;
            FieldProxy field;
        };

        inline std::optional<ConstFieldProxy> at(std::string_view name) const;

        inline auto fieldViews() const{
            return fields | std::views::transform([this](const auto& field){
                return fieldView(field);
            });
        }

        inline auto fieldViews(){
            return fields | std::views::transform([this](const auto& field){
                return fieldView(field);
            });
        }

    private:
        ConstFieldView fieldView(const Field&) const;
        FieldView fieldView(const Field&);
    };
}