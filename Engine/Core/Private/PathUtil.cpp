#include "PathUtil.hpp"

#include <stdexcept>
#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else // Linux
#include <limits.h>
#include <unistd.h>
#endif

namespace Crowy
{
    Str toUTF8String(const std::filesystem::path& path) {
#ifdef __cpp_char8_t
        auto u8str = path.u8string();
        return Str(u8str.begin(), u8str.end());
#else
        return path.u8string();
#endif
    }

#ifdef _WIN32
    std::wstring toUTF16String(StrView utf8Str) {
        auto wlen = MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8Str.data(),
            utf8Str.size(),
            nullptr,
            0
        );
        std::wstring utf16Str(wlen, 0);

        MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8Str.data(),
            utf8Str.size(),
            utf16Str.data(),
            wlen
        );

        return utf16Str;
    }
#endif

    std::filesystem::path toPath(StrView utf8Str) {
#ifdef _WIN32
        return std::filesystem::path(toUTF16String(utf8Str));
#else
        return std::filesystem::path(utf8Str);
#endif
    }

    std::filesystem::path getExecutableDir() {
#if defined(_WIN32)
        // a long path is cut to the buffer, so grow it until the name fits
        std::wstring buffer(MAX_PATH, L'\0');
        while(true) {
            const auto length = GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size())
            );
            if(length == 0)
                throw std::runtime_error("GetModuleFileNameW failed");
            if(length < buffer.size()) {
                buffer.resize(length);
                break;
            }
            buffer.resize(buffer.size() * 2);
        }
        return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
        // a buffer too small fails and reports the size the path needs
        std::string buffer(PATH_MAX, '\0');
        uint32_t size = static_cast<uint32_t>(buffer.size());
        if(_NSGetExecutablePath(buffer.data(), &size) != 0) {
            buffer.resize(size);
            if(_NSGetExecutablePath(buffer.data(), &size) != 0)
                throw std::runtime_error("_NSGetExecutablePath failed");
        }
        return std::filesystem::canonical(buffer.c_str()).parent_path();
#else // Linux
        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, PATH_MAX - 1);
        if(len < 0)
            throw std::runtime_error("readlink /proc/self/exe failed");
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path();
#endif
    }
}
