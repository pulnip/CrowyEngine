#pragma once

#include <filesystem>
#include <format>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Primitives.hpp"

namespace Crowy
{
    inline constexpr bool sameCStr(CStr lhs, CStr rhs) noexcept {
        if(lhs == rhs)
            return true;
        if(lhs == nullptr || rhs == nullptr)
            return false;

        return StrView{lhs} == StrView{rhs};
    }

    Str toUpper(StrView);

    struct StringHash {
        using is_transparent = void;

        usize operator()(StrView view) const {
            return std::hash<StrView>{}(view);
        }
    };
    template<typename T>
    using StringHashMap =
        std::unordered_map<Str, T, StringHash, std::equal_to<>>;

    Str readFileAsString(const std::filesystem::path&);
    std::vector<u8> readFileAsBinary(const std::filesystem::path&);

    std::string toUTF8String(const std::filesystem::path& path);
#ifdef _WIN32
    std::wstring toUTF16String(std::string_view utf8Str);
#endif

    std::filesystem::path getExecutableDir();
    std::filesystem::path getAbsolutePath(const std::filesystem::path&);

    std::filesystem::path toPath(const char* utf8Str);
}

template<>
struct std::formatter<std::filesystem::path>: std::formatter<std::string> {
    inline auto format(const std::filesystem::path& p, auto& ctx) const {
        auto u8str = Crowy::toUTF8String(p);
        return std::formatter<std::string>::format(u8str, ctx);
    }
};
