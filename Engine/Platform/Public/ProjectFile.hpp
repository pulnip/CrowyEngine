#pragma once

#include <filesystem>

#include "DOM.hpp"
#include "DomTraits.hpp"

namespace Crowy
{
    // Crowy.project at the project root. A key comes with the code that
    // reads it, so an unknown one is an error rather than a key nobody reads.
    struct ProjectFile {
        // the content root, relative to the project root and inside it
        std::filesystem::path content;
    };

    template<>
    struct DomTraits<ProjectFile> {
        static ProjectFile from(const DOM::Value&);
    };

    // read once, from projectRoot()
    const ProjectFile& projectFile();
    // a relative path is taken from the content root, an absolute one kept
    std::filesystem::path resolveContentPath(const std::filesystem::path&);
}
