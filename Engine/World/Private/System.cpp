#include "System.hpp"

#include <format>
#include <set>
#include <source_location>

#include "Assert.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        [[noreturn]] void refuse(const Str& message) {
            detail::assertFail(
                "a valid schedule",
                std::source_location::current(),
                message
            );
        }

        std::vector<SystemFn> schedulePhase(
            const SystemDescs& systems,
            Phase phase
        ) {
            StringHashMap<usize> waitingOn;
            StringHashMap<std::vector<const SystemDesc*>> dependents;
            StringHashMap<const SystemDesc*> byName;

            for(const auto& system: systems) {
                if(system.phase != phase)
                    continue;

                byName.emplace(system.name, &system);
                waitingOn[system.name] = system.after.size();
                for(const auto& before: system.after)
                    dependents[before].push_back(&system);
            }

            // a sorted set, so the next ready system is the first by name
            std::set<Str> ready;
            for(const auto& [name, count]: waitingOn) {
                if(count == 0)
                    ready.insert(name);
            }

            std::vector<SystemFn> order;
            while(!ready.empty()) {
                const auto name = *ready.begin();
                ready.erase(ready.begin());
                order.push_back(byName.find(name)->second->run);

                const auto next = dependents.find(name);
                if(next == dependents.end())
                    continue;
                for(const auto* dependent: next->second) {
                    if(--waitingOn.find(dependent->name)->second == 0)
                        ready.insert(dependent->name);
                }
            }

            if(order.size() != byName.size()) {
                std::set<Str> stuck;
                for(const auto& [name, count]: waitingOn) {
                    if(count != 0)
                        stuck.insert(name);
                }

                Str names;
                for(const auto& name: stuck)
                    names += names.empty() ? name : ", " + name;
                refuse(std::format("systems in a cycle: {}", names));
            }

            return order;
        }
    }

    PhaseSchedule scheduleSystems(const SystemDescs& systems) {
        StringHashMap<const SystemDesc*> byName;
        for(const auto& system: systems)
            byName.emplace(system.name, &system);

        for(const auto& system: systems) {
            for(const auto& before: system.after) {
                const auto it = byName.find(before);
                if(it == byName.end()) {
                    refuse(
                        std::format(
                            "'{}' runs after '{}', which is no system",
                            system.name,
                            before
                        )
                    );
                }
                if(it->second->phase != system.phase) {
                    refuse(
                        std::format(
                            "'{}' runs after '{}', which runs in another phase",
                            system.name,
                            before
                        )
                    );
                }
            }
        }

        PhaseSchedule schedule;
        for(usize i = 0; i < PhaseCount; ++i){
            const auto phase = static_cast<Phase>(i);

            schedule[i] = schedulePhase(systems, phase);
        }

        return schedule;
    }

    SystemRegistry& SystemRegistry::Global() {
        static SystemRegistry singleton;
        return singleton;
    }

    void SystemRegistry::Add(SystemDesc system) {
        CROWY_ASSERT(system.run != nullptr, "a system needs a function");

        for(const auto& existing: systems) {
            if(existing.name == system.name) {
                refuse(std::format("two systems are named '{}'", system.name));
            }
        }

        systems.push_back(std::move(system));
    }

    SystemBuilder::SystemBuilder(CStr name, Phase phase, SystemFn run)
        : system{.name = name, .phase = phase, .run = run} {}

    SystemBuilder& SystemBuilder::After(CStr name) {
        system.after.emplace_back(name);

        return *this;
    }

    bool SystemBuilder::Build() {
        SystemRegistry::Global().Add(std::move(system));

        return true;
    }
}
