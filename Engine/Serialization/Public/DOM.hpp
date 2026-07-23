#pragma once

#include <concepts>
#include <optional>
#if defined(__cpp_lib_ranges_enumerate)
    #include <ranges>
#else
    #include "View.hpp"
#endif
#include <unordered_map>
#include <variant>
#include <vector>
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    namespace DOM
    {
        class Value;

        using Array = std::vector<Value>;
        using Table = std::unordered_map<Str, Value>;

        class Value{
        private:
            using ArrayRAII = RAII<Array>;
            using TableRAII = RAII<Table>;

            std::variant<
                std::monostate,
                bool,
                i64,
                f64,
                Str,
                ArrayRAII,
                TableRAII
            > value = std::monostate();

        public:
            Value() = default;
            Value(bool v) noexcept: value(v){}
            Value(i64 v) noexcept: value(v){}
            Value(f64 v) noexcept: value(v){}
            Value(Str v):value(v){}
            Value(const char* v):value(Str(v)){}
            Value(Array&& v): value(std::make_unique<Array>(std::move(v))){}
            Value(Table&& v): value(std::make_unique<Table>(std::move(v))){}

            CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(Value)

            enum class Kind{
                None,
                Bool,
                Int,
                Float,
                String,
                Array,
                Table
            };

            Kind kind() const noexcept;

            bool is_none() const noexcept{ return std::holds_alternative<std::monostate>(value); }
            bool is_bool() const noexcept{ return std::holds_alternative<bool>(value); }
            bool is_int() const noexcept{ return std::holds_alternative<i64>(value); }
            bool is_float() const noexcept{ return std::holds_alternative<f64>(value); }
            bool is_string() const noexcept{ return std::holds_alternative<Str>(value); }
            bool is_array() const noexcept{ return std::holds_alternative<ArrayRAII>(value); }
            bool is_table() const noexcept{ return std::holds_alternative<TableRAII>(value); }

            std::optional<bool> asBool() const noexcept;
            // expect like 1, and 1.0 cannot be Integer
            std::optional<i64> asInt() const noexcept;
            // expect like 1.0, also 1 can be Float
            std::optional<f64> asFloat() const noexcept;
            const Str* asString() const noexcept;
            const Array* asArray() const noexcept;
            const Table* asTable() const noexcept;

            const Value* at(StrView path) const noexcept;

            template<typename T>
            std::optional<T> get() const noexcept(
                !std::is_same_v<T, Str> &&
                noexcept(getImpl<T>())
            ){
                if constexpr(std::same_as<T, bool>){
                    return asBool();
                }
                else if constexpr(std::integral<T>){
                    if(auto opt = asInt())
                        return static_cast<T>(*opt);
                }
                else if constexpr(std::floating_point<T>){
                    if(auto opt = asFloat())
                        return static_cast<T>(*opt);
                    else if(auto opt = asInt())
                        return static_cast<T>(*opt);
                }
                else if constexpr(std::same_as<T, Str>){
                    if(auto ptr = asString())
                        return *ptr;
                }
                else{
                    return getImpl<T>();
                }

                return std::nullopt;
            }

            template<typename T>
            std::optional<T> get(StrView path) const noexcept(
                noexcept(get<T>())
            ){
                auto node = at(path);

                return node != nullptr ?
                    node->get<T>() :
                    std::nullopt;
            }

            template<std::invocable<const DOM::Value&> F>
            void forEach(F&& fn) const{
                auto arr = asArray();
                if(arr == nullptr) return;

                for(auto& val: *arr){
                    fn(val);
                }
            }

            template<std::invocable<const DOM::Value&> F>
            void forEach(StrView path, F&& fn) const{
                auto n = at(path);
                if(n == nullptr) return;

                auto arr = n->asArray();
                if(arr == nullptr) return;

                for(auto& val: *arr){
                    fn(val);
                }
            }

            template<std::invocable<isize, const DOM::Value&> F>
            void enumerate(F&& fn) const{
                auto arr = asArray();
                if(arr == nullptr) return;

                for(auto [idx, value]: std::views::enumerate(*arr)){
                    fn(static_cast<isize>(idx), value);
                }
            }

            template<std::invocable<isize, const DOM::Value&> F>
            void enumerate(StrView path, F&& fn) const{
                auto n = at(path);
                if(n == nullptr) return;

                auto arr = n->asArray();
                if(arr == nullptr) return;

                for(auto [idx, value]: std::views::enumerate(*arr)){
                    fn(static_cast<isize>(idx), value);
                }
            }

        private:
            template<typename T>
            std::optional<T> getImpl() const noexcept{
                static_assert(false);
            }
        };

    #define DECLARE_VALUE_GETIMPL_NOEXCEPT(TYPE) \
        template<> \
        std::optional<TYPE> Value::getImpl<TYPE>() const noexcept;

        DECLARE_VALUE_GETIMPL_NOEXCEPT(Vec2)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Vec3)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Vec4)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Size2D)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Transform)

    #undef DECLARE_VALUE_GETIMPL_NOEXCEPT
    }
}
