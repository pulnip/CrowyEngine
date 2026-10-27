#pragma once

#include <algorithm>
#include <concepts>
#include <optional>
#if defined(__cpp_lib_ranges_enumerate)
#include <ranges>
#else
#include "View.hpp"
#endif
#include <utility>
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

        class Table {
        public:
            using Entry = std::pair<Str, Value>;
            using iterator = std::vector<Entry>::iterator;
            using const_iterator = std::vector<Entry>::const_iterator;

        private:
            std::vector<Entry> entries;

        public:
            iterator begin() noexcept;
            iterator end() noexcept;
            const_iterator begin() const noexcept;
            const_iterator end() const noexcept;
            usize size() const noexcept;
            bool empty() const noexcept;
            auto keys() const noexcept;

            iterator find(StrView key) noexcept;
            const_iterator find(StrView key) const noexcept;
            bool contains(StrView key) const noexcept;
            bool erase(StrView key);

            template<typename V>
            std::pair<iterator, bool> emplace(Str key, V&& v);

            template<typename V>
            std::pair<iterator, bool> insert_or_assign(Str key, V&& v);
        };

        class Value {
        private:
            using ArrayRAII = RAII<Array>;
            using TableRAII = RAII<Table>;

            std::variant<
                std::monostate,
                bool,
                i64,
                f64,
                f32,
                Str,
                ArrayRAII,
                TableRAII>
                value = std::monostate();

        public:
            Value() = default;
            Value(bool v) noexcept
                : value(v) {}
            Value(i64 v) noexcept
                : value(v) {}
            Value(f64 v) noexcept
                : value(v) {}
            Value(f32 v) noexcept
                : value(v) {}
            Value(Str v)
                : value(v) {}
            Value(const char* v)
                : value(Str(v)) {}
            Value(Array&& v)
                : value(std::make_unique<Array>(std::move(v))) {}
            Value(Table&& v)
                : value(std::make_unique<Table>(std::move(v))) {}

            CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(Value)

            enum class Kind { None, Bool, Int, Float, String, Array, Table };

            Kind kind() const noexcept;

            bool is_none() const noexcept {
                return std::holds_alternative<std::monostate>(value);
            }
            bool is_bool() const noexcept {
                return std::holds_alternative<bool>(value);
            }
            bool is_int() const noexcept {
                return std::holds_alternative<i64>(value);
            }
            bool is_float() const noexcept {
                return std::holds_alternative<f64>(value) ||
                       std::holds_alternative<f32>(value);
            }
            bool is_string() const noexcept {
                return std::holds_alternative<Str>(value);
            }
            bool is_array() const noexcept {
                return std::holds_alternative<ArrayRAII>(value);
            }
            bool is_table() const noexcept {
                return std::holds_alternative<TableRAII>(value);
            }

            std::optional<bool> asBool() const noexcept;
            std::optional<i64> asInt() const noexcept;
            std::optional<f64> asFloat() const noexcept;
            // only a float that was made from an f32
            std::optional<f32> asSingle() const noexcept;
            const Str* asString() const noexcept;
            const Array* asArray() const noexcept;
            const Table* asTable() const noexcept;
            Array* asArray() noexcept;
            Table* asTable() noexcept;

            const Value* at(StrView path) const noexcept;

            template<typename T>
            std::optional<T> get() const noexcept(
                !std::is_same_v<T, Str> && noexcept(getImpl<T>())
            ) {
                if constexpr(std::same_as<T, bool>) {
                    return asBool();
                }
                else if constexpr(std::integral<T>) {
                    if(auto opt = asInt())
                        return static_cast<T>(*opt);
                }
                else if constexpr(std::floating_point<T>) {
                    if(auto opt = asFloat())
                        return static_cast<T>(*opt);
                    else if(auto opt = asInt())
                        return static_cast<T>(*opt);
                }
                else if constexpr(std::same_as<T, Str>) {
                    if(auto ptr = asString())
                        return *ptr;
                }
                else {
                    return getImpl<T>();
                }

                return std::nullopt;
            }

            template<typename T>
            std::optional<T> get(StrView path) const noexcept(
                noexcept(get<T>())
            ) {
                auto node = at(path);

                return node != nullptr ? node->get<T>() : std::nullopt;
            }

            // the write half of get<T>: builds the Value get<T> reads back
            template<typename T>
            static Value from(const T& v) {
                if constexpr(std::same_as<T, bool>) {
                    return Value(v);
                }
                else if constexpr(std::integral<T>) {
                    return Value(static_cast<i64>(v));
                }
                else if constexpr(std::floating_point<T>) {
                    // an f32 stays one, so it writes as its own shortest text
                    return Value(v);
                }
                else if constexpr(std::same_as<T, Str>) {
                    return Value(v);
                }
                else {
                    return fromImpl<T>(v);
                }
            }

            template<std::invocable<const DOM::Value&> F>
            void forEach(F&& fn) const {
                auto arr = asArray();
                if(arr == nullptr)
                    return;

                for(auto& val: *arr) {
                    fn(val);
                }
            }

            template<std::invocable<const DOM::Value&> F>
            void forEach(StrView path, F&& fn) const {
                auto n = at(path);
                if(n == nullptr)
                    return;

                auto arr = n->asArray();
                if(arr == nullptr)
                    return;

                for(auto& val: *arr) {
                    fn(val);
                }
            }

            template<std::invocable<isize, const DOM::Value&> F>
            void enumerate(F&& fn) const {
                auto arr = asArray();
                if(arr == nullptr)
                    return;

                for(auto [idx, value]: std::views::enumerate(*arr)) {
                    fn(static_cast<isize>(idx), value);
                }
            }

            template<std::invocable<isize, const DOM::Value&> F>
            void enumerate(StrView path, F&& fn) const {
                auto n = at(path);
                if(n == nullptr)
                    return;

                auto arr = n->asArray();
                if(arr == nullptr)
                    return;

                for(auto [idx, value]: std::views::enumerate(*arr)) {
                    fn(static_cast<isize>(idx), value);
                }
            }

        private:
            template<typename T>
            std::optional<T> getImpl() const noexcept {
                static_assert(false);
            }

            template<typename T>
            static Value fromImpl(const T&) {
                static_assert(false);
            }
        };

        inline Table::iterator Table::begin() noexcept {
            return entries.begin();
        }
        inline Table::iterator Table::end() noexcept {
            return entries.end();
        }
        inline Table::const_iterator Table::begin() const noexcept {
            return entries.begin();
        }
        inline Table::const_iterator Table::end() const noexcept {
            return entries.end();
        }
        inline usize Table::size() const noexcept {
            return entries.size();
        }
        inline bool Table::empty() const noexcept {
            return entries.empty();
        }
        inline auto Table::keys() const noexcept {
            return entries | std::views::keys;
        }

        inline Table::iterator Table::find(StrView key) noexcept {
            return std::ranges::find(entries, key, &Entry::first);
        }
        inline Table::const_iterator Table::find(StrView key) const noexcept {
            return std::ranges::find(entries, key, &Entry::first);
        }
        inline bool Table::contains(StrView key) const noexcept {
            return find(key) != end();
        }
        inline bool Table::erase(StrView key) {
            const auto it = find(key);
            if(it == end())
                return false;

            entries.erase(it);
            return true;
        }

        template<typename V>
        std::pair<Table::iterator, bool> Table::emplace(Str key, V&& v) {
            if(auto it = find(key); it != end())
                return {it, false};

            entries.emplace_back(std::move(key), Value(std::forward<V>(v)));

            return {std::prev(entries.end()), true};
        }

        template<typename V>
        std::pair<Table::iterator, bool> Table::insert_or_assign(
            Str key,
            V&& v
        ) {
            if(auto it = find(key); it != end()) {
                it->second = Value(std::forward<V>(v));

                return {it, false};
            }

            return emplace(std::move(key), std::forward<V>(v));
        }

#define DECLARE_VALUE_GETIMPL_NOEXCEPT(TYPE)                                   \
    template<>                                                                 \
    std::optional<TYPE> Value::getImpl<TYPE>() const noexcept;

        DECLARE_VALUE_GETIMPL_NOEXCEPT(Vec2)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Vec3)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Vec4)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Size2D)
        DECLARE_VALUE_GETIMPL_NOEXCEPT(Transform)

#undef DECLARE_VALUE_GETIMPL_NOEXCEPT

#define DECLARE_VALUE_FROMIMPL(TYPE)                                           \
    template<>                                                                 \
    Value Value::fromImpl<TYPE>(const TYPE&);

        DECLARE_VALUE_FROMIMPL(Vec2)
        DECLARE_VALUE_FROMIMPL(Vec3)
        DECLARE_VALUE_FROMIMPL(Vec4)
        DECLARE_VALUE_FROMIMPL(Size2D)
        DECLARE_VALUE_FROMIMPL(Transform)

#undef DECLARE_VALUE_FROMIMPL
    }
}
