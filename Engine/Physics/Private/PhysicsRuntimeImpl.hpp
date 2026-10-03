#pragma once

#include <memory>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

#include "PhysicsRuntime.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // Jolt's allocator, hooks, factory and registered types, set up before
    // anything else allocates through them and torn down last
    class JoltGlobals {
    private:
        std::unique_ptr<JPH::Factory> factory;

    public:
        ~JoltGlobals();
        CROWY_DECLARE_PINNED(JoltGlobals)

        JoltGlobals();
    };

    class PhysicsRuntime::Impl {
    public:
        JoltGlobals globals;
        JPH::TempAllocatorImpl tempAllocator;
        JPH::JobSystemThreadPool jobSystem;
        usize worldCount = 0;

    public:
        ~Impl();
        CROWY_DECLARE_PINNED(Impl)

        explicit Impl(const PhysicsRuntimeDesc& desc);
    };
}
