#pragma once

#include <functional>
#include <utility>

#include "Primitives.hpp"

namespace Crowy
{
    struct TypeDesc;

    // a write access may record a change, so it is asked for only at the
    // moment of writing
    enum class TargetAccess : u8 {
        Read,
        Write,
    };

    // What a port target or a panel section edits, found again at every
    // access, so a row that moved or went away is never reached through a
    // stale address.
    class PropertyTarget {
    public:
        // nullptr when what the target names is gone
        using Resolver = std::function<void*(TargetAccess)>;

    private:
        Resolver resolver;
        const TypeDesc* desc = nullptr;

    public:
        PropertyTarget(Resolver resolver, const TypeDesc& desc)
            : resolver(std::move(resolver)), desc(&desc) {}

        // an object whose address never changes
        static PropertyTarget Fixed(void* object, const TypeDesc& desc) {
            return PropertyTarget(
                [object](TargetAccess) { return object; },
                desc
            );
        }

        void* Resolve(TargetAccess access) const { return resolver(access); }

        const TypeDesc& Desc() const noexcept { return *desc; }
    };
}
