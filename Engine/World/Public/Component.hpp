#pragma once

#include <functional>
#include <type_traits>
#include <unordered_map>

#include "Actor.hpp"
#include "ClassRegistry.hpp"

namespace Crowy
{
    class World;

    using ComponentHook = std::function<void(World&, ActorId, void*)>;

    // a reflected struct the World keeps in a pool per actor
    struct ComponentInfo {
        const TypeDesc* desc = nullptr;
        // at Sync: after the row lands, and before it leaves
        ComponentHook onAdd;
        ComponentHook onRemove;
    };

    // every component linked in, registered by CROWY_COMPONENT at static init
    class ComponentRegistry {
    private:
        std::unordered_map<const TypeDesc*, ComponentInfo> infoOfType;

    public:
        static ComponentRegistry& Get();

        // a type registered twice stops the process in every build
        void Register(ComponentInfo info);

        // nullptr for a type no CROWY_COMPONENT registered
        const ComponentInfo* Find(const TypeDesc& desc) const noexcept;
    };

    template<typename T>
        requires(!std::is_base_of_v<Object, T>)
    class ComponentBuilder
        : public DescBuilder<ComponentBuilder<T>, T, StructDesc> {
        using Base = DescBuilder<ComponentBuilder<T>, T, StructDesc>;
        using Hook = void (*)(World&, ActorId, T&);

        ComponentInfo info;

    public:
        ComponentBuilder()
            : Base(ClassRegistry::Get().DescFor<T>()) {}

        ComponentBuilder& OnAdd(Hook hook) {
            info.onAdd = typed(hook);

            return *this;
        }

        ComponentBuilder& OnRemove(Hook hook) {
            info.onRemove = typed(hook);

            return *this;
        }

        bool Build() {
            static_assert(
                std::is_move_constructible_v<T> && std::is_destructible_v<T>,
                "a component lives in a pool that moves its rows"
            );

            ClassRegistry::Get().Register(this->desc);
            info.desc = &this->desc;
            ComponentRegistry::Get().Register(std::move(info));

            return true;
        }

    private:
        static ComponentHook typed(Hook hook) {
            return [hook](World& world, ActorId actor, void* component) {
                hook(world, actor, *static_cast<T*>(component));
            };
        }
    };
}

// clang-format off
// Component registration start macro: a Struct the World can pool, which
// takes SetProperty, OnAdd and OnRemove
#define CROWY_COMPONENT(TYPE) \
namespace{ \
    const auto Is##TYPE##Registered = ::Crowy::ComponentBuilder<TYPE>() \
        .SetName(#TYPE)

// Component registration end macro
#define CROWY_COMPONENT_END(TYPE) \
        .Build(); \
}
// clang-format on
