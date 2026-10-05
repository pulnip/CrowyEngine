#include "PhysicsRuntime.hpp"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/RegisterTypes.h>

#include "Assert.hpp"
#include "LogLocal.hpp"
#include "PhysicsRuntimeImpl.hpp"

namespace
{
    bool runtimeAlive = false;

    void traceToLog(const char* format, ...) {
        std::array<char, 1024> text{};
        va_list args;
        va_start(args, format);
        std::vsnprintf(text.data(), text.size(), format, args);
        va_end(args);
        // error level goes to unbuffered stderr, which survives the abort
        // some traces precede
        LOG_ERROR("Jolt: {}", text.data());
    }

#ifdef JPH_ENABLE_ASSERTS
    // never returns true, which would break into a debugger and kill a
    // headless run without a word
    bool failJoltAssert(
        const char* expression,
        const char* message,
        const char* file,
        JPH::uint line
    ) {
        std::fprintf(
            stderr,
            "Assertion failed: %s\n  %s:%u (Jolt)\n  %s\n",
            expression,
            file,
            line,
            message != nullptr ? message : ""
        );
        std::fflush(stderr);
        std::abort();
    }
#endif
}

namespace Crowy
{
    JoltGlobals::JoltGlobals() {
        [[maybe_unused]] const auto wasAlive =
            std::exchange(runtimeAlive, true);
        CROWY_ASSERT(!wasAlive, "a PhysicsRuntime already exists");

        JPH::RegisterDefaultAllocator();
        JPH::Trace = traceToLog;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = failJoltAssert;)

        factory = std::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = factory.get();
        JPH::RegisterTypes();
    }

    JoltGlobals::~JoltGlobals() {
        // the allocator and the hooks stay: Jolt has no way to remove them,
        // and its defaults would assert or break into a debugger
        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
        factory.reset();
        runtimeAlive = false;
    }

    PhysicsRuntime::Impl::Impl(const PhysicsRuntimeDesc& desc)
        : tempAllocator(desc.tempAllocatorBytes),
          jobSystem(
              JPH::cMaxPhysicsJobs,
              JPH::cMaxPhysicsBarriers,
              static_cast<int>(desc.workerThreads)
          ) {}

    PhysicsRuntime::Impl::~Impl() = default;

    PhysicsRuntime::PhysicsRuntime(const PhysicsRuntimeDesc& desc)
        : impl(std::make_unique<Impl>(desc)) {}

    PhysicsRuntime::~PhysicsRuntime() {
        CROWY_ASSERT(
            impl->worldCount == 0,
            "the PhysicsRuntime still has {} worlds",
            impl->worldCount
        );
    }
}
