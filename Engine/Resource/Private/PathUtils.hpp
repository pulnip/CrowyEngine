#pragma once

#include <filesystem>
#include <format>

template<>
struct std::formatter<std::filesystem::path>: std::formatter<std::string>{
    inline auto format(const std::filesystem::path& p, auto& ctx) const{
    #ifdef __cpp_char8_t
        auto u8str = p.u8string();
        return std::formatter<std::string>::format(
            std::string(u8str.begin(), u8str.end()), ctx
        );
    #else
        return std::formatter<std::string>::format(p.u8string(), ctx);
    #endif
    }
};

namespace Crowy
{
    std::filesystem::path resolveAssetPath(const std::filesystem::path&);

    std::filesystem::path toPath(const char* utf8Str);
    std::string toUtf8String(const std::filesystem::path&);
}