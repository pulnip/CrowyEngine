#include "ProjectFile.hpp"

#include <format>
#include <stdexcept>

#include "JsonLoader.hpp"
#include "ProjectRoot.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        std::filesystem::path contentOf(const DOM::Value& value) {
            const auto* text = value.asString();
            if(text == nullptr || text->empty())
                throw std::runtime_error("'content' needs a folder name");

            const auto content = toPath(text->c_str()).lexically_normal();
            const bool escapes = !content.empty() && *content.begin() == "..";
            if(content.has_root_path() || escapes) {
                throw std::runtime_error(
                    std::format(
                        "'content' is '{}', not a folder inside the project",
                        *text
                    )
                );
            }
            return content;
        }

        ProjectFile readProjectFile() {
            const auto path = projectRoot() / ProjectFileName;
            try {
                return DomTraits<ProjectFile>::from(parseJsonFile(path));
            }
            catch(const std::exception& e) {
                throw std::runtime_error(std::format("{}: {}", path, e.what()));
            }
        }
    }

    ProjectFile DomTraits<ProjectFile>::from(const DOM::Value& root) {
        const auto* table = root.asTable();
        if(table == nullptr)
            throw std::runtime_error("a project file is a JSON object");

        for(const auto& [key, value]: *table) {
            if(key != "content")
                throw std::runtime_error(std::format("unknown key '{}'", key));
        }
        const auto content = table->find("content");
        if(content == table->end())
            throw std::runtime_error("'content' is missing");

        return ProjectFile{.content = contentOf(content->second)};
    }

    const ProjectFile& projectFile() {
        static const auto file = readProjectFile();
        return file;
    }

    std::filesystem::path resolveContentPath(
        const std::filesystem::path& path
    ) {
        if(path.is_absolute())
            return path;
        return projectRoot() / projectFile().content / path;
    }
}
