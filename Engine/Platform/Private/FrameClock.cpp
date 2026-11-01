#include "FrameClock.hpp"

#include <chrono>
#include <utility>

#include "FixedTickClock.hpp"

namespace Crowy
{
    namespace
    {
        constexpr Timer::Duration FixedStep{
            std::chrono::nanoseconds(TickNanoseconds)
        };
    }

    bool FrameClock::Advance() {
        const auto gate =
            quitRequested ? FrameControl::Gate::Free : control.Advance(frame);
        if(gate == FrameControl::Gate::Hold) {
            held = true;
            return false;
        }

        if(std::exchange(held, false))
            timer.Rebase();
        stepped = gate == FrameControl::Gate::Step;
        return true;
    }

    void FrameClock::Tick() noexcept {
        if(stepped || fixedSteps)
            timer.Step(FixedStep);
        else
            timer.NewFrame();
    }
}
