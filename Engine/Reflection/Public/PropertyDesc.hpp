#pragma once

#include <optional>
#include <tuple>
#include "Primitives.hpp"
#include "TypeInfo.hpp"

namespace Crowy
{
    struct PropertyAccessor{
        virtual ~PropertyAccessor() = default;
        virtual void* Get(void* obj) const = 0;
    };

    namespace detail
    {
        template<typename T>
        struct MemberPointerTraits;

        template<typename Class, typename Member>
        struct MemberPointerTraits<Member Class::*>{
            using Owner = Class;
            using Member_ = Member;
        };

        // Resolves a chain of member pointers(&A::b, &B::c, &C::d)
        // into its root class(A) and leaf type(D).
        template<typename... Ptrs>
        struct MemberChain;

        template<typename Ptr>
        struct MemberChain<Ptr>{
            using Root = typename MemberPointerTraits<Ptr>::Owner;
            using Leaf = typename MemberPointerTraits<Ptr>::Member_;
        };

        template<typename Ptr, typename... Rest>
        struct MemberChain<Ptr, Rest...>{
            using Root = typename MemberPointerTraits<Ptr>::Owner;
            using Leaf = typename MemberChain<Rest...>::Leaf;

            static_assert(
                std::is_base_of_v<
                    typename MemberChain<Rest...>::Root,
                    typename MemberPointerTraits<Ptr>::Member_
                >,
                "member pointer chain is disconnected"
            );
        };

        template<typename... Ptrs>
        concept MemberPointers =
            sizeof...(Ptrs) > 0 &&
            (std::is_member_object_pointer_v<Ptrs> && ...);

        // the chain must start at Class, or at one of its bases
        template<typename Class, typename... Ptrs>
        concept MemberChainOf =
            MemberPointers<Ptrs...> &&
            std::is_base_of_v<typename MemberChain<Ptrs...>::Root, Class>;
    }

    // Walks an arbitrarily deep member pointer chain:
    //   MemberAccessor{&A::b}              -> A::b
    //   MemberAccessor{&A::b, &B::c}       -> A::b.c
    //   MemberAccessor{&A::b, &B::c, ...}  -> A::b.c...
    template<typename... Ptrs>
        requires detail::MemberPointers<Ptrs...>
    struct MemberAccessor: public PropertyAccessor{
    private:
        using Root = typename detail::MemberChain<Ptrs...>::Root;

        std::tuple<Ptrs...> ptrs;

    public:
        explicit MemberAccessor(Ptrs... ptrs)
            : ptrs(ptrs...){}

        void* Get(void* obj) const override{
            return Resolve<0>(static_cast<Root*>(obj));
        }

    private:
        template<usize I, typename Owner>
        void* Resolve(Owner* owner) const{
            if constexpr(I == sizeof...(Ptrs)){
                return owner;
            }
            else{
                return Resolve<I + 1>(&(owner->*std::get<I>(ptrs)));
            }
        }
    };

    struct PropertyMeta{
        std::optional<std::pair<f32, f32>> range = std::nullopt;
        CStr tooltip = nullptr;
    };

    struct PropertyDesc{
        const TypeInfo& typeInfo;
        RAII<PropertyAccessor> accessor;
        PropertyMeta meta;
    };
}
