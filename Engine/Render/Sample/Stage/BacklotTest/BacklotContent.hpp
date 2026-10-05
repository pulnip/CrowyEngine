#pragma once

#include <filesystem>

#include "StringUtil.hpp"

namespace Crowy
{
    // the checkout the suite reads; the binary exits 77 before any test
    // runs when it is missing or holds Git LFS pointers
    std::filesystem::path backlotRoot();
}
