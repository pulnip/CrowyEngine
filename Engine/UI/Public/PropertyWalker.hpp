#pragma once

#include "ClassRegistry.hpp"
#include "Primitives.hpp"
#include "PropertyTarget.hpp"
#include "PropertyWrite.hpp"
#include "Widget.hpp"

namespace Crowy
{
    // Values are seeded at build, so a write that bypasses the tree shows once
    // the caller rebuilds it; an edit resolves the target again at that moment.
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
