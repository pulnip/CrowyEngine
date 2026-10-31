#pragma once

#include <filesystem>
#include <map>
#include <vector>

#include "Primitives.hpp"
#include "StageLoad.hpp"

// Backlot's film set as engine scene content: one scene file, material files
// and the models and images they name, all under one folder of the content
// mount. Nothing else in Scene sees the Stage reader.
namespace Crowy
{
    struct BacklotImport;
    struct ImportedCopy;

    using ImportedCopies = std::vector<ImportedCopy>;
    // by path relative to the output folder
    using ImportedTexts = std::map<Str, Str>;

    // the content mount's folder every ref the import writes starts with
    inline constexpr CStr BacklotFolder = "Backlot";
    inline constexpr CStr BacklotSceneFile = "Backlot.json";
    inline constexpr CStr BacklotSourceFile = "Source/scene.json";

    // The scene file the default lighting key shows, as bytes: no file is
    // touched. Throws std::runtime_error when two quads would share a
    // material file but not its contents.
    BacklotImport importBacklot(const LoadedStage& stage);
    // the texts written, the copies made, the folders created
    void writeBacklotImport(
        const BacklotImport& import,
        const std::filesystem::path& out
    );

    struct ImportedCopy {
        // relative to the Backlot checkout
        Str from;
        // relative to the output folder
        Str to;
    };

    struct BacklotImport {
        std::filesystem::path root;
        ImportedTexts texts;
        ImportedCopies copies;
        // what the import left out, one line each; the same for the same set
        Str report;
    };
}
