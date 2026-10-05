#pragma once

#include <memory>

#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct PhysicsRuntimeDesc {
        // 0 runs every job on the thread that steps
        u32 workerThreads = 0;
        usize tempAllocatorBytes = 10 * 1024 * 1024;
    };

    // Jolt's process-wide state, its job system and its scratch memory: one
    // per process, outliving every world
    class PhysicsRuntime {
    public:
        class Impl;

    private:
        friend class PhysicsWorld;

    private:
        std::unique_ptr<Impl> impl;

    public:
        ~PhysicsRuntime();
        CROWY_DECLARE_PINNED(PhysicsRuntime)

        explicit PhysicsRuntime(const PhysicsRuntimeDesc& desc = {});
    };
}
