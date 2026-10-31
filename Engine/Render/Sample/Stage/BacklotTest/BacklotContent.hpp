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

    inline constexpr usize BacklotInstances = 1489;
    inline constexpr usize BacklotQuads = 179;
    // every key shows the same quads
    inline constexpr usize BacklotKeyedQuads = 161;
    inline constexpr std::array BacklotKeys{
        BacklotKeyInventory{
            .key = "day",
            .instances = 1329,
            .triangles = 469790,
            .lights = 0
        },
        BacklotKeyInventory{
            .key = "evening",
            .instances = 1440,
            .triangles = 472430,
            .lights = 31
        },
        BacklotKeyInventory{
            .key = "sunset",
            .instances = 1440,
            .triangles = 472430,
            .lights = 31
        },
        BacklotKeyInventory{
            .key = "night",
            .instances = BacklotInstances,
            .triangles = 473894,
            .lights = 31
        },
    };

    // the checkout the suite reads
    std::filesystem::path backlotRoot();
}
