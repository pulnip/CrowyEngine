#pragma once

#include <filesystem>

#include "DOM.hpp"
#include "World.hpp"

namespace Crowy
{
    // The World's actors as a scene document: parents before children, each
    // with its file id, name, parent, local transform and saved components.
    // Throws, naming every component it cannot write.
    DOM::Value saveScene(const World& world);
    // Into an empty World, as of its next Sync. Throws with every problem the
    // document has, and then no actor of it lands.
    void loadScene(World& world, DOM::Value scene);

    void saveSceneFile(const World& world, const std::filesystem::path& path);
    void loadSceneFile(World& world, const std::filesystem::path& path);
}
