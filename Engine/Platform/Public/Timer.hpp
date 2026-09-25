#pragma once

#include <chrono>
#include "Primitives.hpp"

namespace Crowy
{
    // Frame time: the wall clock between frames, or a step a frame is told
    // it took
    class Timer{
    public:
        using Duration = std::chrono::high_resolution_clock::duration;

    private:
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = std::chrono::time_point<Clock, Duration>;

        TimePoint lastPoint;

        // the last frame's, from NewFrame or Step
        Duration deltaTime = Duration(0);
        // every frame's since construction; a Rebase adds nothing
        Duration elapsedTime = Duration(0);

    public:
        Timer() noexcept;
        ~Timer() = default;

        // this frame took the wall-clock time since the last one
        void NewFrame() noexcept;
        // this frame took `step`, whatever the clock says
        void Step(Duration step) noexcept;
        // the time since the last frame is nobody's: a hold, or OnInit
        void Rebase() noexcept;

        f64 GetDeltaTime() const noexcept;
        f64 GetElapsedTime() const noexcept;
    };
}
