#pragma once

#include "ClassRegistry.hpp"
#include "Primitives.hpp"
#include "PropertyTarget.hpp"
#include "PropertyWrite.hpp"
#include "Widget.hpp"

namespace Crowy
{
    // Builds one target's section:
    //   parent-chain properties first,
    //   then own properties in declaration order,
    //   each mapped to a widget by leaf type,
    //   a list to its elements and a + and - that change its length.
    // Values are seeded at build time and the tree is rebuilt
    // only when the target set changes,
    // so a write that bypasses the tree desyncs the display
    // until the next rebuild. An edit resolves the target again, so it
    // lands on the object as it is then, or nowhere if it is gone.
    Widget buildPropertyTree(
        CStr label,
        PropertyTarget target,
        DirtyCallback onDirty
    );
    // an object whose address never changes
    Widget buildPropertyTree(
        CStr label,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty
    );
}
