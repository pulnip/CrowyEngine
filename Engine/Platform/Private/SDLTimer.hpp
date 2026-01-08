#pragma once

#include "SDL3/SDL_timer.h"

namespace Crowy
{
    class SDLTimer{
    private:
        Uint64 freq;
        Uint64 prev;

        bool paused = false;

        float deltaTime = 0.0f;
        float totalTime = 0.0f;
        float fps       = 0.0f;
        float timeScale = 1.0f;

    public:
        inline SDLTimer()
            :freq(SDL_GetPerformanceFrequency())
            ,prev(SDL_GetPerformanceCounter())
        {}

        inline void reset(){
            freq = SDL_GetPerformanceFrequency();
            prev = SDL_GetPerformanceCounter();

            deltaTime = totalTime = fps = 0.0f;
        }
        inline void newFrame(){
            auto now = SDL_GetPerformanceCounter();
            auto elapsed = now - prev;
            prev = now;

            if(paused){
                deltaTime = 0.0f;
                return;
            }

            deltaTime = static_cast<float>(elapsed) / freq;

            constexpr auto maxDeltaTime = 0.1f;
            if(deltaTime > maxDeltaTime){
                deltaTime = maxDeltaTime;
            }

            totalTime += timeScale*deltaTime;
            fps = deltaTime>0.0f ? 1.0f/deltaTime : 0.0f;
        }

        inline void pause(){
            paused = true;
        }
        inline void resume(){
            if(paused){
                paused = false;
                prev = SDL_GetPerformanceCounter();
            }
        }
        inline bool isPaused() const{
            return paused;
        }

        inline float getDeltaTime() const{ return deltaTime; }
        inline float getTotalTime() const{ return totalTime; }
        inline float getFPS()       const{ return fps;       }
        inline float getTimeScale() const{ return timeScale; }
        inline float getScaledDeltaTime() const{
            return timeScale*deltaTime;
        }

        inline void setTimeScale(float scale){ timeScale = scale; }
    };
}