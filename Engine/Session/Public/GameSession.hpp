#pragma once

#include "Primitives.hpp"
#include "WorldSession.hpp"

namespace Crowy
{
    // A WorldSession on every system there is, advanced by a game clock of
    // its own: the wall clock times the scale, and nothing while paused.
    class GameSession: public WorldSession {
    private:
        f64 timeScale = 1.0;
        bool paused = false;

    public:
        GameSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        );

        // the scale and the pause hold for the frames after this call
        void SetTimeScale(f64 scale) noexcept { timeScale = scale; }
        f64 TimeScale() const noexcept { return timeScale; }
        void SetPaused(bool pause) noexcept { paused = pause; }
        bool Paused() const noexcept { return paused; }

        void Frame(f64 realSeconds);
    };
}
