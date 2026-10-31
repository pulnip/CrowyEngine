#pragma once

#include <vector>

#include "Primitives.hpp"
#include "PropertyWalker.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // Shows every Quat of a property tree as Euler degrees: x pitch, y yaw,
    // z roll. What was typed stays on show while the rotation is still the one
    // it produced; any other rotation decomposes afresh.
    class EulerInspector {
    private:
        struct TypedEuler {
            Vec3 degrees{};
            Quat rotation{};
        };

        // one entry per rotation; written only when someone types
        std::vector<TypedEuler> typed;

    public:
        EulerInspector() = default;
        ~EulerInspector() = default;
        CROWY_DECLARE_PINNED(EulerInspector)

        // the widgets it builds keep pointing at this inspector
        ValueOverrides Overrides();

    private:
        Vec3 degreesShown(Quat rotation) const;
        void remember(Vec3 degrees, Quat rotation);
        Widget rotationWidget(const ValueSite& site);
    };
}
