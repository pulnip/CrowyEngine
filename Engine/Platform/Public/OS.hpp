#pragma once

#include "FastPimpl.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class MainLoop;
    class InputProvider;
    class Window;

    struct RuntimeConfig;

    class OS{
    private:
        class Impl;
        static constexpr usize implSize = 640;
        static constexpr usize implAlign = 8;
        FastPimpl<Impl, implSize, implAlign> impl;

        static OS* singleton;

    public:
        OS(const RuntimeConfig&, RHIDevice&);
        ~OS();
        CROWY_DECLARE_PINNED(OS)

        void Run(MainLoop&, RHIDevice&);

        inline static OS* Get(){ return singleton; }

        const InputProvider& GetInputProvider() const noexcept;
        const Window& GetWindow() const noexcept;
    };

    // direct access to ptr is unsafe, but handy helper
    #define OS_ *OS::Get()
}
