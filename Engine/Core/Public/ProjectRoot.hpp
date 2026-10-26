#pragma once

#include <filesystem>
#include <optional>

namespace Crowy
{
    inline constexpr auto ProjectFileName = "Crowy.project";

    // the nearest folder at or above start that holds the project file
    std::optional<std::filesystem::path> findProjectRoot(
        const std::filesystem::path& start
    );

    // found once, from the executable's folder: engine files resolve against
    // it however the process was launched, never against the cwd
    const std::filesystem::path& projectRoot();
    void setProjectRoot(const std::filesystem::path& folder);

    // a relative path is taken from projectRoot(), an absolute one kept
    std::filesystem::path resolveProjectPath(const std::filesystem::path&);
}
