#pragma once

#include "ClassRegistry.hpp"
#include "Primitives.hpp"
#include "PropertyWrite.hpp"
#include "Widget.hpp"

namespace Crowy
{
    // Builds one target's section:
    //   parent-chain properties first,
    //   then own properties in declaration order,
    //   each mapped to a widget by leaf type.
    // Values are seeded at build time and the tree is rebuilt
    // only when the target set changes,
    // so a write that bypasses the tree desyncs the display
    // until the next rebuild.
    Widget buildPropertyTree(
        CStr label,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty
    );
}
