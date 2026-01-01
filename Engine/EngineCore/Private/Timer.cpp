#include "Timer.hpp"

namespace Crowy
{
    void Timer::reset(){
        start = prev = Clock::now();
        delta = elapsed = Seconds{0};
        frameIndex = 0;
    }

    void Timer::newFrame(){
        auto now = Clock::now();
        prev = now;
        delta = now - prev;
        auto d = rawDeltaSeconds();

        if(delta.count() > maxDeltaSeconds)
            delta = Seconds{maxDeltaSeconds};

        fps = 0.9f * fps + 0.1f * (1.0f / d);

        elapsed = now - start;
        ++frameIndex;
    }
}