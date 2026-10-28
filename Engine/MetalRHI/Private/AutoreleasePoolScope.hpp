#pragma once

#include <Foundation/NSAutoreleasePool.hpp>

#include "MetalDefinitions.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class AutoreleasePoolScope {
    private:
        RefPtr<NS::AutoreleasePool> pool;

    public:
        AutoreleasePoolScope()
            : pool(NS::TransferPtr(NS::AutoreleasePool::alloc()->init())) {}

        ~AutoreleasePoolScope() = default;

        CROWY_DECLARE_PINNED(AutoreleasePoolScope)
    };
}
