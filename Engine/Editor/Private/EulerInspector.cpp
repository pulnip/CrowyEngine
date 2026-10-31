#include "EulerInspector.hpp"

#include <algorithm>

#include "LinearAlgebra.hpp"

namespace Crowy
{
    ValueOverrides EulerInspector::Overrides() {
        return ValueOverrides{
            {getTypeOps<Quat>(),
             [this](const ValueSite& site) { return rotationWidget(site); }}
        };
    }

    Vec3 EulerInspector::degreesShown(Quat rotation) const {
        const auto found = std::ranges::find(
            typed,
            static_cast<Vec4>(rotation),
            [](const TypedEuler& entry) {
                return static_cast<Vec4>(entry.rotation);
            }
        );
        if(found != typed.end())
            return found->degrees;

        const auto euler = eulerOf(rotation);
        return Vec3{degreesOf(euler.x), degreesOf(euler.y), degreesOf(euler.z)};
    }

    void EulerInspector::remember(Vec3 degrees, Quat rotation) {
        std::erase_if(typed, [&](const TypedEuler& entry) {
            return entry.rotation == rotation;
        });
        typed.push_back(TypedEuler{degrees, rotation});
    }

    Widget EulerInspector::rotationWidget(const ValueSite& site) {
        return Float3Field{
            .label = site.label,
            .onChanged =
                [this, site](UIContext&, Vec3 degrees) {
                    site.edit([&](void* value) {
                        const Quat rotation = quatOf(
                            Vec3{
                                radiansOf(degrees.x),
                                radiansOf(degrees.y),
                                radiansOf(degrees.z)
                            }
                        );
                        *static_cast<Quat*>(value) = rotation;
                        remember(degrees, rotation);
                    });
                },
            .v = degreesShown(*static_cast<const Quat*>(site.value))
        };
    }
}
