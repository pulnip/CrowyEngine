#pragma once

#include <chrono>
#include <cstdint>

namespace Crowy
{
    class Timer{
    public:
        using Clock = std::chrono::steady_clock;
        using Seconds = std::chrono::duration<float>;

        void reset();
        void newFrame();

        inline float deltaSeconds() const{
            return paused ? delta.count() * timeScale : 0.0f;
        }
        inline float rawDeltaSeconds() const{ return delta.count(); }
        inline float elapsedSeconds() const{ return elapsed.count(); }
        inline float getFrameIndex() const{ return frameIndex; }
        inline float getFPS() const{ return fps; }

        inline void setTimeScale(float scale){ timeScale = scale; }
        inline float getTimeScale() const{ return timeScale; }

        void setPaused(bool paused){ this->paused = paused; }
        bool getPaused() const{ return paused; }

    private:
        Clock::time_point start{};
        Clock::time_point prev{};
        Seconds delta{};
        Seconds elapsed{};
        std::uint64_t frameIndex = 0;

        float timeScale = 1.0f;
        bool paused = false;
        float maxDeltaSeconds = 0.1f;
        float fps = 60.0f;
    };
}