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
}
