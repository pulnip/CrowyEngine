#pragma once

#include <memory>

#include "InputEvent.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class MainLoop;
    class InputProvider;

    struct RuntimeConfig;

    class OS {
    private:
        class Impl;
        RAII<Impl> impl;

        static OS* singleton;

    public:
        OS(const RuntimeConfig&, RHIDevice&);
        ~OS();
        CROWY_DECLARE_PINNED(OS)

        void Run(MainLoop&, RHIDevice&);

        inline static OS* Get() { return singleton; }

        const InputProvider& GetInputProvider() const noexcept;
        InputStatus ReadInput() const noexcept;

        // a --port-input run's input, taken by the next frame that advances
        void PushInput(const InputEvent&);
    };

#define OS_ *OS::Get()
}
