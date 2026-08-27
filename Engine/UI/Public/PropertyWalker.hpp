#pragma once

#include <functional>
#include "ClassRegistry.hpp"
#include "Primitives.hpp"
#include "Widget.hpp"

namespace Crowy
{
    // Fired by the tree after every write it performs.
    // Every writer must notify through its target's callback
    // - a future writer (the remote port) fires this same path,
    // and the moment a second writer exists the
    // shared write-then-notify helper gets hoisted into Reflection.
    using DirtyCallback = std::function<void()>;

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
