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
        u64 frameNumber = 0;

        bool paused = false;

        // i = frameNumber
        // time between [i-1, i]
        Duration deltaTime = Duration(0);
        // time between [0, i]
        Duration elapsedTime = Duration(0);

        f64 scale = 1.0f;

    public:
        Timer() noexcept;
        ~Timer() = default;

        void NewFrame() noexcept;
        void Reset() noexcept;

        void SetPaused(bool v) noexcept{ paused = v; }
        bool isPaused() const noexcept{ return paused; }

        u64 GetFrameNumber() const noexcept{ return frameNumber; }
        f64 GetDeltaTime() const noexcept;
        f64 GetElapsedTime() const noexcept;
        f64 GetFPS() const noexcept;

        void SetScale(f64 scale = 1.0) noexcept;
    };
}
