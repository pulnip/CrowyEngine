#include "Timer.hpp"
#include <chrono>

namespace Crowy
{
    Timer::Timer() noexcept
        : lastPoint(Clock::now()){}

    void Timer::NewFrame() noexcept{
        if(paused) [[unlikely]] return;

        auto now = Clock::now();

        deltaTime = now - lastPoint;
        elapsedTime += deltaTime;

        lastPoint = now;
    }

    void Timer::Reset() noexcept{
        lastPoint = Clock::now();
        deltaTime = elapsedTime = Duration(0);
    }

    f64 Timer::GetDeltaTime() const noexcept{
        using namespace std::chrono;

        auto sec = duration<f64>(deltaTime);
        return scale * sec.count();
    }

    f64 Timer::GetElapsedTime() const noexcept{
        using namespace std::chrono;

        auto sec = duration<f64>(elapsedTime);
        return scale * sec.count();
    }

    void Timer::SetScale(f64 scale) noexcept{
        this->scale = scale;
    }
}
