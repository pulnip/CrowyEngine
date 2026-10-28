#pragma once

#include <array>
#include <filesystem>

#include "StringUtil.hpp"

namespace Crowy
{
    // the rows each lighting key shows
    struct BacklotKeyInventory {
        CStr key = "";
        usize instances = 0;
        u64 triangles = 0;
        usize lights = 0;
    };

    inline constexpr usize BacklotInstances = 1488;
    inline constexpr usize BacklotQuads = 179;
    // every key shows the same quads
    inline constexpr usize BacklotKeyedQuads = 161;
    inline constexpr std::array BacklotKeys{
        BacklotKeyInventory{
            .key = "day",
            .instances = 1328,
            .triangles = 466462,
            .lights = 0
        },
        BacklotKeyInventory{
            .key = "evening",
            .instances = 1439,
            .triangles = 469102,
            .lights = 31
        },
        BacklotKeyInventory{
            .key = "sunset",
            .instances = 1439,
            .triangles = 469102,
            .lights = 31
        },
        BacklotKeyInventory{
            .key = "night",
            .instances = BacklotInstances,
            .triangles = 470566,
            .lights = 31
        },
    };

    // the checkout the suite reads
    std::filesystem::path backlotRoot();
}
