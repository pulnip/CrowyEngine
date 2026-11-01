#include "GameSession.hpp"

#include <utility>

namespace Crowy
{
    GameSession::GameSession(RAII<RenderSession> render)
        : WorldSession(std::move(render), SystemRegistry::Global()) {}

    void GameSession::Frame(f64 realSeconds) {
        GetWorld().Frame(realSeconds, paused ? 0.0 : realSeconds * timeScale);
    }
}
