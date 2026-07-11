#pragma once

#include <memory>
#include <type_traits>
#include "Primitives.hpp"

namespace Crowy
{
    template<typename T>
    void* ptrAdd(void* ptr){
        return static_cast<u8*>(ptr) + sizeof(T);
    }
    template<typename T>
    const void* ptrAdd(const void* ptr){
        return static_cast<const u8*>(ptr) + sizeof(T);
    }
    inline void* ptrAdd(void* ptr, usize nbyte){
        return static_cast<u8*>(ptr) + nbyte;
    }
    inline const void* ptrAdd(const void* ptr, usize nbyte){
        return static_cast<const u8*>(ptr) + nbyte;
    }

    template<typename T>
    void* ptrSub(void* ptr){
        return static_cast<u8*>(ptr) - sizeof(T);
    }
    template<typename T>
    const void* ptrSub(const void* ptr){
        return static_cast<const u8*>(ptr) - sizeof(T);
    }
    inline void* ptrSub(void* ptr, usize nbyte){
        return static_cast<u8*>(ptr) - nbyte;
    }
    inline const void* ptrSub(const void* ptr, usize nbyte){
        return static_cast<const u8*>(ptr) - nbyte;
    }

    template <typename Derived, typename Base>
        requires std::is_base_of_v<Base, Derived>
    std::unique_ptr<Derived> uniqueCast(std::unique_ptr<Base>& base){
        if(Derived* raw = dynamic_cast<Derived*>(base.get())){
            base.release();
            return std::unique_ptr<Derived>(raw);
        }

        return nullptr;
    }
}
