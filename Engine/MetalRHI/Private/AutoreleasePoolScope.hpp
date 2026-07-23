#pragma once

#include <Foundation/NSAutoreleasePool.hpp>
#include "Semantics.hpp"

namespace Crowy
{
    class AutoreleasePoolScope{
    private:
        NS::AutoreleasePool* pool = nullptr;

    public:
        AutoreleasePoolScope()
            : pool(NS::AutoreleasePool::alloc()->init()){}

        ~AutoreleasePoolScope(){
            if(pool != nullptr){
                pool->release();
                pool = nullptr;
            }
        }

        CROWY_DECLARE_PINNED(AutoreleasePoolScope)
    };
}
