#pragma once

#include <functional>
#include <unordered_map>

#include "ClassRegistry.hpp"
#include "Primitives.hpp"
#include "PropertyTarget.hpp"
#include "PropertyWrite.hpp"
#include "Widget.hpp"

namespace Crowy
{
    using ValueChange = std::function<void(void*)>;
    using ValueEdit = std::function<void(const ValueChange&)>;

    // A value as the walker meets it, for a widget that replaces the one the
    // walker would build.
    struct ValueSite {
        Str label;
        // as of the build
        const void* value = nullptr;
        // runs the change on the value as it is then and notifies the tree's
        // caller; nothing happens when the value is gone
        ValueEdit edit;
    };

    using ValueWidgetBuilder = std::function<Widget(const ValueSite&)>;
    // a builder is called at build only; what its widget captures is the
    // caller's to keep alive
    using ValueOverrides =
        std::unordered_map<const TypeOps*, ValueWidgetBuilder>;

    // Values are seeded at build, so a write that bypasses the tree shows once
    // the caller rebuilds it; an edit resolves the target again at that moment.
    Widget buildPropertyTree(
        CStr label,
        PropertyTarget target,
        DirtyCallback onDirty,
        const ValueOverrides& overrides = {}
    );
    // an object whose address never changes
    Widget buildPropertyTree(
        CStr label,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty,
        const ValueOverrides& overrides = {}
    );
}
