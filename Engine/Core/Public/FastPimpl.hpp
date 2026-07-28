#pragma once

#include <new>
#include <utility>
#include <type_traits>
#include "Primitives.hpp"

namespace Crowy
{
    template <class T, usize Size, usize Align>
    class FastPimpl{
    public:
        FastPimpl(){
            ::new (raw()) T();
        }

        template<class First, class... Rest,
            class = std::enable_if_t<!std::is_same_v<FastPimpl, std::decay_t<First>>>
        >
        explicit FastPimpl(First&& first, Rest&&... rest){
            ::new (raw()) T(std::forward<First>(first), std::forward<Rest>(rest)...);
        }

        FastPimpl(const FastPimpl& o){
            ::new (raw()) T(*o);
        }
        FastPimpl(FastPimpl&& o) noexcept{
            ::new (raw()) T(std::move(*o));
        }

        FastPimpl& operator=(const FastPimpl& o){
            **this = *o;
            return *this;
        }
        FastPimpl& operator=(FastPimpl&& o) noexcept{
            **this = std::move(*o);
            return *this;
        }

        ~FastPimpl() {
            validate<sizeof(T), alignof(T)>();
            (**this).~T();
        }

        T*       operator->()       noexcept{ return  get(); }
        const T* operator->() const noexcept{ return  get(); }
        T&       operator* ()       noexcept{ return *get(); }
        const T& operator* () const noexcept{ return *get(); }

    private:
        template <usize ActualSize, usize ActualAlign>
        static void validate() noexcept{
            static_assert(Size >= ActualSize);
            static_assert(Align % ActualAlign == 0);
        }

        void* raw() noexcept{
            return static_cast<void*>(storage_);
        }

        T* get() noexcept{
            return std::launder(
                reinterpret_cast<T*>(storage_)
            );
        }
        const T* get() const noexcept{
            return std::launder(
                reinterpret_cast<const T*>(storage_)
            );
        }

        alignas(Align) u8 storage_[Size];
    };
}
