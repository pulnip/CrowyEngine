#pragma once

#include "generic_handle.hpp"

namespace Crowy
{
    using         MeshHandle = generic_handle<struct         Mesh>;
    using  MaterialSetHandle = generic_handle<struct  MaterialSet>;
    using       ScriptHandle = generic_handle<struct EntityScript>;
}