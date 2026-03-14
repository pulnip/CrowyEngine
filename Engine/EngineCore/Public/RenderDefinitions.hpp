#pragma once

#include <cstddef>
#include <cstdint>
#ifdef __cpp_exceptions
#include <stdexcept>
#else
#include <cstdlib>
#endif
#include <cstring>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "assert.hpp"
#include "math.hpp"
#include "ptr_util.hpp"
#include "string.hpp"

#define DEFINE_CONST_OP(Type, Op) \
    template<typename T> \
        requires std::is_trivially_copyable_v<T> \
    friend auto operator Op(Type lhs, const T& rhs) noexcept{ \
        CROWY_ASSERT(size_of(lhs.type) == sizeof(T)); \
        T t = lhs; \
        return t Op rhs; \
    } \
    template<typename T> \
        requires std::is_trivially_copyable_v<T> \
    friend auto operator Op(const T& lhs, Type rhs) noexcept{ \
        CROWY_ASSERT(sizeof(T) == size_of(rhs.type)); \
        T t = rhs; \
        return lhs Op t; \
    }
#define DEFINE_ALL_CONST_OP(Type) \
    DEFINE_CONST_OP(Type, ==) \
    DEFINE_CONST_OP(Type, <=>) \
    DEFINE_CONST_OP(Type, +) \
    DEFINE_CONST_OP(Type, -) \
    DEFINE_CONST_OP(Type, *) \
    DEFINE_CONST_OP(Type, /) \
    DEFINE_CONST_OP(Type, %) \
    DEFINE_CONST_OP(Type, &) \
    DEFINE_CONST_OP(Type, |) \
    DEFINE_CONST_OP(Type, ^) \
    DEFINE_CONST_OP(Type, <<) \
    DEFINE_CONST_OP(Type, >>) \
    DEFINE_CONST_OP(Type, &&)
#define DEFINE_MUTABLE_OP(Type, Op) \
    template<typename T> \
        requires std::is_trivially_copyable_v<T> \
    friend Type operator Op(Type lhs, const T& rhs) noexcept{ \
        CROWY_ASSERT(size_of(lhs.type) == sizeof(T)); \
        T t = lhs; \
        t Op rhs; \
        lhs = t; \
        return lhs; \
    } \
    template<typename T> \
        requires std::is_trivially_copyable_v<T> \
    friend auto operator Op(T& lhs, Type rhs) noexcept{ \
        CROWY_ASSERT(sizeof(T) == size_of(rhs.type)); \
        T t = rhs; \
        return lhs Op t; \
    }
#define DEFINE_ALL_MUTABLE_OP(Type) \
    DEFINE_MUTABLE_OP(Type, +=) \
    DEFINE_MUTABLE_OP(Type, -=) \
    DEFINE_MUTABLE_OP(Type, *=) \
    DEFINE_MUTABLE_OP(Type, /=) \
    DEFINE_MUTABLE_OP(Type, %=) \
    DEFINE_MUTABLE_OP(Type, &=) \
    DEFINE_MUTABLE_OP(Type, |=) \
    DEFINE_MUTABLE_OP(Type, ^=) \
    DEFINE_MUTABLE_OP(Type, <<=) \
    DEFINE_MUTABLE_OP(Type, >>=)
#define DEFINE_ALL_OP(Type) \
    DEFINE_ALL_CONST_OP(Type) \
    DEFINE_ALL_MUTABLE_OP(Type)

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
        case CBufferFieldType::Float3:   return 12;
        case CBufferFieldType::Float4:   return 16;
        case CBufferFieldType::Float4x4: return 64;
        default:
            std::unreachable();
        }
    }
    size_t stride_of(CBufferFieldType);
    using CBufferFieldOffset = size_t;

    struct CBufferFieldMeta{
        CBufferFieldType type;
        size_t count = 1;
        CBufferFieldOffset offset = 0;
    };

    struct CBufferConstFieldProxy{
        const CBufferFieldType type;
        const std::vector<std::byte>& buffer;
        size_t offset;

        auto data(this auto& self){ return &self.buffer[self.offset]; }

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        operator T() const noexcept{
            CROWY_ASSERT(size_of(type) == sizeof(T));
            T t;
            std::memcpy(&t, data(), sizeof(T));

            return t;
        }

        friend bool operator==(CBufferConstFieldProxy lhs, CBufferConstFieldProxy rhs) noexcept{
            CROWY_ASSERT(size_of(lhs.type) == size_of(rhs.type));
            return std::memcmp(&lhs.buffer[lhs.offset], &rhs.buffer[rhs.offset], size_of(lhs.type)) == 0;
        }

        DEFINE_ALL_CONST_OP(CBufferConstFieldProxy)
    };

    struct CBufferFieldProxy{
        const CBufferFieldType type;
        std::vector<std::byte>& buffer;
        size_t offset;

        auto data(this auto& self){ return &self.buffer[self.offset]; }

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        operator T() const{
            CROWY_ASSERT(size_of(type) == sizeof(T));
            T t;
            std::memcpy(&t, data(), sizeof(T));

            return t;
        }

        inline operator CBufferConstFieldProxy(){
            return CBufferConstFieldProxy{
                .type = type,
                .buffer = buffer,
                .offset = offset
            };
        }

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        CBufferFieldProxy& operator=(const T& t){
            CROWY_ASSERT(sizeof(T) == size_of(type));
            std::memcpy(data(), &t, sizeof(T));

            return *this;
        }

        inline CBufferFieldProxy& operator=(const CBufferFieldProxy& other){
            CROWY_ASSERT(size_of(type) == size_of(other.type));

            if(data() == other.data())
                return *this;

            std::memcpy(data(), other.data(), size_of(type));
            return *this;
        }

        inline CBufferFieldProxy& operator=(const CBufferConstFieldProxy& other){
            if(data() == other.data())
                return *this;

            CROWY_ASSERT(size_of(type) == size_of(other.type));
            std::memcpy(data(), other.data(), size_of(type));
            return *this;
        }

        inline friend void swap(CBufferFieldProxy lhs, CBufferFieldProxy rhs){
            if(lhs.data() == rhs.data())
                return;

            CROWY_ASSERT(size_of(lhs.type) == size_of(rhs.type));
            constexpr size_t BUF_SIZE = 256;
            std::byte tmp[BUF_SIZE];
            const auto size = size_of(lhs.type);

            void* plhs = lhs.data();
            void* prhs = rhs.data();
            auto remain = size;

            while(remain > 0){
                auto s = std::min(BUF_SIZE, remain);
                std::memcpy(tmp, plhs, size);
                std::memcpy(plhs, prhs, size);
                std::memcpy(prhs, tmp, size);

                plhs = ptr_add(plhs, s);
                prhs = ptr_add(prhs, s);
                remain -= s;
            }
        }

        friend bool operator==(CBufferFieldProxy lhs, CBufferFieldProxy rhs) noexcept{
            CROWY_ASSERT(size_of(lhs.type) == size_of(rhs.type));
            return std::memcmp(lhs.data(), rhs.data(), size_of(lhs.type)) == 0;
        }
        friend bool operator==(CBufferFieldProxy lhs, CBufferConstFieldProxy rhs) noexcept{
            CROWY_ASSERT(size_of(lhs.type) == size_of(rhs.type));
            return std::memcmp(lhs.data(), rhs.data(), size_of(lhs.type)) == 0;
        }
        friend bool operator==(CBufferConstFieldProxy lhs, CBufferFieldProxy rhs) noexcept{
            CROWY_ASSERT(size_of(lhs.type) == size_of(rhs.type));
            return std::memcmp(lhs.data(), rhs.data(), size_of(lhs.type)) == 0;
        }

        DEFINE_ALL_OP(CBufferFieldProxy)
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
        // buffer size without last padding
        size_t fieldLast = 0;

        inline void reserve(size_t n){ buffer.reserve(n); }

        // count == 0 for non-array type
        inline auto newField(
            std::string_view name,
            FieldType type,
            size_t count = 0
        ){
            auto [it, succeed] = fieldIndex.try_emplace(std::string(name), fields.size());
            if(succeed){
                auto offset = nextFieldOffset(fieldLast, type);

                fields.push_back({
                    .name = std::string(name),
                    .meta = {
                        .type = type,
                        .count = count,
                        .offset = offset
                    }
                });

                if(count == 0)
                    fieldLast = offset + size_of(type);
                else
                    fieldLast = offset + stride_of(type) * count;

                // 16byte alignment for buffer
                auto bufSize = (fieldLast + 15) & ~size_t{15};
                if(bufSize > buffer.size())
                    buffer.resize(bufSize);
            }

            auto& field = fields[it->second];
            return FieldProxy{
                .type = field.meta.type,
                .buffer = buffer,
                .offset = field.meta.offset
            };
        }

        inline auto at(this auto& self, std::string_view name, size_t index = 0){
            auto it = self.fieldIndex.find(name);
            if(it == self.fieldIndex.end())
        #ifdef __cpp_exceptions
                throw std::out_of_range("CBuffer::at");
        #else
                std::abort();
        #endif

            auto& field = self.fields[it->second];
            CROWY_ASSERT(field.meta.count==0 || index < field.meta.count);
            auto type = field.meta.type;
            if constexpr(std::is_const_v<
                std::remove_reference_t<decltype(self)>
            >)
                return ConstFieldProxy{
                    .type = type,
                    .buffer = self.buffer,
                    .offset = field.meta.offset + index * stride_of(type)
                };
            else
                return FieldProxy{
                    .type = type,
                    .buffer = self.buffer,
                    .offset = field.meta.offset + index*stride_of(type)
                };
        }

        inline auto fieldViews(this auto& self){
            return self.fields | std::views::transform(
                [&self](const auto& field){
                    if constexpr(std::is_const_v<
                        std::remove_reference_t<decltype(self)>
                    >)
                        return ConstFieldView{
                            field.name, ConstFieldProxy{
                            .type = field.meta.type,
                            .buffer = self.buffer,
                            .offset = field.meta.offset
                        }};
                    else
                        return FieldView{
                            field.name, FieldProxy{
                            .type = field.meta.type,
                            .buffer = self.buffer,
                            .offset = field.meta.offset
                        }};
                }
            );
        }

    private:
        static size_t nextFieldOffset(size_t current, FieldType type){
            size_t size = size_of(type);

            if(type == CBufferFieldType::Float4x4){
                return (current + 15) & ~size_t{15};
            }

            // 4byte alignment for each field
            current = (current + 3) & ~size_t{3};

            // 16byte alignment for boundary
            size_t last = (current & ~size_t{15}) + 16;
            if(current + size > last){
                current = last;
            }

            return current;
        }
    };
}

#undef DEFINE_CONST_OP
#undef DEFINE_ALL_CONST_OP
#undef DEFINE_MUTABLE_OP
#undef DEFINE_ALL_MUTABLE_OP
#undef DEFINE_ALL_OP
