#pragma once

#include <filesystem>
#include <format>
#include <string>

#include "Primitives.hpp"

namespace Crowy
{
    Str toUTF8String(const std::filesystem::path& path);
#ifdef _WIN32
    std::wstring toUTF16String(StrView utf8Str);
#endif

    std::filesystem::path toPath(StrView utf8Str);

    std::filesystem::path getExecutableDir();
}

template<>
struct std::formatter<std::filesystem::path>: std::formatter<std::string> {
    inline auto format(const std::filesystem::path& p, auto& ctx) const {
        auto u8str = Crowy::toUTF8String(p);
        return std::formatter<std::string>::format(u8str, ctx);
    }
};
