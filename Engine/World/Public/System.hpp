#pragma once

#include <array>
#include <vector>

#include "Primitives.hpp"

namespace Crowy
{
    class World;

    // the frame's fixed order; the FixedUpdate phases run once per fixed tick
    enum class Phase : u8 {
        Input,
        PreFixedUpdate,
        Physics,
        PostFixedUpdate,
        Update,
        Sync,
        Extract,
    };

    inline constexpr usize PhaseCount = static_cast<usize>(Phase::Extract) + 1;

    struct FrameTime {
        // the wall clock's
        f64 realSeconds = 0.0;
        // the game's; in a FixedUpdate phase, one fixed tick
        f64 gameSeconds = 0.0;
        // fixed ticks run so far, the running one included
        u64 tick = 0;
    };

    using SystemFn = void (*)(World&, const FrameTime&);
    using SystemNames = std::vector<Str>;

    struct SystemDesc {
        Str name;
        Phase phase = Phase::Update;

        SystemFn run = nullptr;
        // systems of the same phase that run before this one
        SystemNames after;
        // play, not editing: a World that only edits leaves it out
        bool gameplay = false;
    };

    using SystemDescs = std::vector<SystemDesc>;
    using PhaseSchedule = std::array<std::vector<SystemFn>, PhaseCount>;

    // every `after` before its system, ties by name; an unknown name, one in
    // another phase or a cycle stops the process in every build
    PhaseSchedule scheduleSystems(const SystemDescs& systems);

    class SystemRegistry {
    private:
        SystemDescs systems;

    public:
        // every system linked in, registered by CROWY_SYSTEM at static init
        static SystemRegistry& Global();

        // a name registered twice stops the process in every build
        void Add(SystemDesc system);

        const SystemDescs& All() const noexcept { return systems; }

        // a registry of every system that is not gameplay
        SystemRegistry WithoutGameplay() const;
    };

    class SystemBuilder {
    private:
        SystemDesc system;

    public:
        SystemBuilder(CStr name, Phase phase, SystemFn run);

        SystemBuilder& After(CStr name);
        SystemBuilder& Gameplay();

        bool Build();
    };
}

// clang-format off
// NAME is the system's name, which After names; FN is a SystemFn
#define CROWY_SYSTEM(NAME, PHASE, FN) \
namespace{ \
    const auto Is##NAME##SystemRegistered = \
        ::Crowy::SystemBuilder(#NAME, ::Crowy::Phase::PHASE, FN)

#define CROWY_SYSTEM_END(NAME) \
        .Build(); \
}
// clang-format on
