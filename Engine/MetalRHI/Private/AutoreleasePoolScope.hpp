#pragma once

#include <Foundation/NSAutoreleasePool.hpp>

namespace Crowy
{
    struct AutoreleasePoolScope{
        NS::AutoreleasePool* pool = nullptr;

        AutoreleasePoolScope()
            : pool(NS::AutoreleasePool::alloc()->init()){}

        ~AutoreleasePoolScope(){
            if(pool != nullptr){
                pool->release();
                pool = nullptr;
            }
        }

        AutoreleasePoolScope(const AutoreleasePoolScope&) = delete;
        AutoreleasePoolScope& operator=(const AutoreleasePoolScope&) = delete;
        AutoreleasePoolScope(AutoreleasePoolScope&&) = delete;
        AutoreleasePoolScope& operator=(AutoreleasePoolScope&&) = delete;
    };
}