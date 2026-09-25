#include "Timer.hpp"
#include <chrono>

namespace Crowy
{
    Timer::Timer() noexcept
        : lastPoint(Clock::now()){}

    void Timer::NewFrame() noexcept{
        auto now = Clock::now();

        deltaTime = now - lastPoint;
        elapsedTime += deltaTime;

        lastPoint = now;
    }

    void Timer::Step(Duration step) noexcept{
        deltaTime = step;
        elapsedTime += step;

        // so the next wall-clock frame starts from here, not from before it
        lastPoint = Clock::now();
    }

    void Timer::Rebase() noexcept{
        lastPoint = Clock::now();
    }

    f64 Timer::GetDeltaTime() const noexcept{
        using namespace std::chrono;

        auto sec = duration<f64>(deltaTime);
        return sec.count();
    }

    f64 Timer::GetElapsedTime() const noexcept{
        using namespace std::chrono;

        auto sec = duration<f64>(elapsedTime);
        return sec.count();
    }
}
