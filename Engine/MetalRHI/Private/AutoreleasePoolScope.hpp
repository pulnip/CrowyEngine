#pragma once

#include <Foundation/NSAutoreleasePool.hpp>
#include <Foundation/NSSharedPtr.hpp>
#include "Semantics.hpp"

namespace Crowy
{
    class AutoreleasePoolScope{
    private:
        NS::SharedPtr<NS::AutoreleasePool> pool;

    public:
        AutoreleasePoolScope()
            : pool(NS::TransferPtr(
                NS::AutoreleasePool::alloc()->init()
            ))
        {}

        ~AutoreleasePoolScope() = default;

        CROWY_DECLARE_PINNED(AutoreleasePoolScope)
    };
}
