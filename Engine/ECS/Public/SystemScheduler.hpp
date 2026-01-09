#pragma once

#include <memory>
#include <vector>
#include "semantics.hpp"
#include "System.hpp"

namespace Crowy
{
    class EntityRegistry;

    template<typename Context>
    class SystemScheduler{
    private:
        using SystemPtr = std::unique_ptr<System<Context>>;
        std::vector<SystemPtr> systems;

    public:
        SystemScheduler() = default;
        ~SystemScheduler() = default;
        CROWY_DECLARE_PINNED(SystemScheduler)

        void attach(SystemPtr system){
            systems.push_back(std::move(system));
        }
        void update(EntityRegistry& registry, Context& ctx){
            for(const auto& system: systems){
                system->update(registry, ctx);
            }
        }
    };
}