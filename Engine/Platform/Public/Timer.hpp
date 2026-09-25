#pragma once

#include <chrono>
#include "Primitives.hpp"

namespace Crowy
{
    class Timer{
    private:
        using Clock = std::chrono::high_resolution_clock;
        using Duration = Clock::duration;
        using TimePoint = std::chrono::time_point<Clock, Duration>;

        TimePoint lastPoint;

        bool paused = false;

        // between the last two NewFrame calls
        Duration deltaTime = Duration(0);
        // since construction or Reset
        Duration elapsedTime = Duration(0);

        f64 scale = 1.0f;

    public:
        Timer() noexcept;
        ~Timer() = default;

        void NewFrame() noexcept;
        void Reset() noexcept;

        void SetPaused(bool v) noexcept{ paused = v; }
        bool isPaused() const noexcept{ return paused; }

        f64 GetDeltaTime() const noexcept;
        f64 GetElapsedTime() const noexcept;

        void SetScale(f64 scale = 1.0) noexcept;
    };
}
