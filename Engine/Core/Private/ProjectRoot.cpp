#include "ProjectRoot.hpp"

#include <format>
#include <mutex>
#include <stdexcept>

#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        struct ProjectRootState {
            std::mutex mutex;
            std::optional<std::filesystem::path> root;
            bool read = false;
        };

        ProjectRootState& projectRootState() {
            static ProjectRootState state;
            return state;
        }
    }

    std::optional<std::filesystem::path> findProjectRoot(
        const std::filesystem::path& start
    ) {
        auto folder = std::filesystem::absolute(start).lexically_normal();
        while(true) {
            if(std::filesystem::is_regular_file(folder / ProjectFileName))
                return folder;
            if(folder == folder.parent_path())
                return std::nullopt;
            folder = folder.parent_path();
        }
    }

    const std::filesystem::path& projectRoot() {
        auto& state = projectRootState();
        const std::scoped_lock lock(state.mutex);
        if(!state.root) {
            const auto executableDir = getExecutableDir();
            state.root = findProjectRoot(executableDir);
            if(!state.root) {
                throw std::runtime_error(
                    std::format(
                        "no {} at or above '{}'; pass --project <folder>",
                        ProjectFileName,
                        executableDir
                    )
                );
            }
        }
        state.read = true;
        return *state.root;
    }

    void setProjectRoot(const std::filesystem::path& folder) {
        auto& state = projectRootState();
        const std::scoped_lock lock(state.mutex);
        if(state.read) {
            throw std::logic_error(
                std::format(
                    "the project root was already read as '{}'",
                    *state.root
                )
            );
        }
        const auto absolute =
            std::filesystem::absolute(folder).lexically_normal();
        if(!std::filesystem::is_regular_file(absolute / ProjectFileName)) {
            throw std::runtime_error(
                std::format("'{}' holds no {}", absolute, ProjectFileName)
            );
        }
        state.root = absolute;
    }

    std::filesystem::path resolveProjectPath(
        const std::filesystem::path& path
    ) {
        if(path.is_absolute())
            return path;
        return projectRoot() / path;
    }
}
