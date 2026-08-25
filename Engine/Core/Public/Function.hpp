// for std::move_only_function
#pragma once

#include <functional>

#if !defined(__cpp_lib_move_only_function)

#include <memory>
#include <type_traits>
#include <utility>

namespace std
{
    template<typename...>
    class move_only_function;

    template<typename R, typename... Args>
    class move_only_function<R(Args...)>{
        struct callable{
            virtual ~callable() = default;
            virtual R invoke(Args&&... args) = 0;
        };

        template<typename F>
        struct bound_callable final: callable{
            F fn;

            explicit bound_callable(F&& fn)
                : fn(std::move(fn)){}
            explicit bound_callable(const F& fn)
                : fn(fn){}

            R invoke(Args&&... args) override{
                return std::invoke(fn, std::forward<Args>(args)...);
            }
        };

        std::unique_ptr<callable> bound;

    public:
        move_only_function() noexcept = default;
        move_only_function(std::nullptr_t) noexcept{}

        template<typename F>
            requires(
                !std::is_same_v<std::remove_cvref_t<F>, move_only_function> &&
                std::is_invocable_r_v<R, std::decay_t<F>&, Args...>
            )
        move_only_function(F&& fn)
            : bound(std::make_unique<bound_callable<std::decay_t<F>>>(
                  std::forward<F>(fn)
              )){}

        move_only_function& operator=(std::nullptr_t) noexcept{
            bound.reset();
            return *this;
        }

        R operator()(Args... args){
            return bound->invoke(std::forward<Args>(args)...);
        }

        explicit operator bool() const noexcept{
            return static_cast<bool>(bound);
        }

        friend bool operator==(
            const move_only_function& fn, std::nullptr_t
        ) noexcept{
            return !fn;
        }

        void swap(move_only_function& other) noexcept{
            bound.swap(other.bound);
        }
    };
}

#endif
