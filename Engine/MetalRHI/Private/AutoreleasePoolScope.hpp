#pragma once

#include <Foundation/NSAutoreleasePool.hpp>

namespace Crowy
{
    struct AutoreleasePoolScope{
        NS::AutoreleasePool* pool;

        AutoreleasePoolScope()
            : pool(NS::AutoreleasePool::alloc()->init()){}

        ~AutoreleasePoolScope(){
            pool->release();
        }

        AutoreleasePoolScope(const AutoreleasePoolScope&) = delete;
        AutoreleasePoolScope& operator=(const AutoreleasePoolScope&) = delete;
        AutoreleasePoolScope(AutoreleasePoolScope&&) = delete;
        AutoreleasePoolScope& operator=(AutoreleasePoolScope&&) = delete;
    };
}