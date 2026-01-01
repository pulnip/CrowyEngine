#include <filesystem>
#include "PathUtils.hpp"

#ifdef __APPLE__
    #include <mach-o/dyld.h>
#elif _WIN32
    #include <Windows.h>
#else // Linux
    #include <unistd.h>
    #include <limits.h>
#endif

namespace Crowy
{
    std::filesystem::path getExecutableDir(){
    #ifdef __APPLE__
        char buffer[PATH_MAX];
        uint32_t size = PATH_MAX;
        _NSGetExecutablePath(buffer, &size);
        return std::filesystem::canonical(buffer).parent_path();
    #elif _WIN32
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer).parent_path();
    #else // Linux
        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, PATH_MAX - 1);
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path();
    #endif
    }

    std::filesystem::path resolveAssetPath(const std::filesystem::path& path){
        if(path.is_absolute())
            return path;

        return getExecutableDir() / path;
    }

    std::filesystem::path toPath(const char* utf8Str){
    #ifdef _WIN32
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, nullptr, 0);
        std::wstring wide(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wide.data(), len);
        return std::filesystem::path(wide);
    #else
        return std::filesystem::path(utf8Str);
    #endif
    }

    std::string toUtf8String(const std::filesystem::path& path){
    #ifdef __cpp_char8_t
        auto u8str = path.u8string();
        return std::string(u8str.begin(), u8str.end());
    #else
        return path.u8string();
    #endif
    }
}
