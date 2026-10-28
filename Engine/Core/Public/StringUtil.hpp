#pragma once

#include <algorithm>
#include <charconv>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <unordered_map>

#include "Primitives.hpp"

namespace Crowy
{
    template<typename T>
    concept Number =
        (std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>;

    inline constexpr bool sameCStr(CStr lhs, CStr rhs) noexcept {
        if(lhs == rhs)
            return true;
        if(lhs == nullptr || rhs == nullptr)
            return false;

        return StrView{lhs} == StrView{rhs};
    }

    template<std::ranges::input_range R, typename Proj = std::identity>
    inline constexpr Str join(R&& tokens, StrView sep = {}, Proj proj = {}) {
        Str joined;
        bool first = true;

        for(auto&& token: tokens) {
            if(first) {
                first = false;
            }
            else {
                joined += sep;
            }

            joined += std::invoke(proj, token);
        }

        return joined;
    }

    inline constexpr bool consumePrefix(
        StrView& text,
        StrView prefix
    ) noexcept {
        if(!text.starts_with(prefix))
            return false;

        text.remove_prefix(prefix.size());
        return true;
    }

    inline constexpr Str replaceAll(StrView text, StrView from, StrView to) {
        if(from.empty())
            return Str(text);

        Str replaced;
        usize at = 0;
        for(auto found = text.find(from); found != StrView::npos;
            found = text.find(from, at)) {
            replaced += text.substr(at, found - at);
            replaced += to;
            at = found + from.size();
        }
        replaced += text.substr(at);

        return replaced;
    }

    template<Number T>
    inline constexpr std::optional<T> parseNumber(StrView text) {
        T value{};
        const auto* end = text.data() + text.size();
        const auto [ptr, error] = std::from_chars(text.data(), end, value);
        if(error != std::errc{} || ptr != end)
            return std::nullopt;

        return value;
    }

    inline constexpr char lowerChar(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    inline constexpr Str lower(StrView text) {
        Str lowered(text);
        std::ranges::transform(lowered, lowered.begin(), lowerChar);

        return lowered;
    }

    inline constexpr bool equalsIgnoringCase(StrView lhs, StrView rhs) {
        return std::ranges::equal(lhs, rhs, {}, lowerChar, lowerChar);
    }

    inline constexpr bool containsIgnoringCase(StrView text, StrView part) {
        return part.empty() ||
               !std::ranges::search(text, part, {}, lowerChar, lowerChar)
                    .empty();
    }

    struct StringHash {
        using is_transparent = void;

        usize operator()(StrView view) const {
            return std::hash<StrView>{}(view);
        }
    };
    template<typename T>
    using StringHashMap =
        std::unordered_map<Str, T, StringHash, std::equal_to<>>;
}
