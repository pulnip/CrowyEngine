#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    template<typename T>
    void* ptrAdd(void* ptr) {
        return static_cast<u8*>(ptr) + sizeof(T);
    }
    template<typename T>
    const void* ptrAdd(const void* ptr) {
        return static_cast<const u8*>(ptr) + sizeof(T);
    }
    inline void* ptrAdd(void* ptr, usize nbyte) {
        return static_cast<u8*>(ptr) + nbyte;
    }
    inline const void* ptrAdd(const void* ptr, usize nbyte) {
        return static_cast<const u8*>(ptr) + nbyte;
    }
}
