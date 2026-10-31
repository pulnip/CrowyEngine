#include "GameSession.hpp"

namespace Crowy
{
    GameSession::GameSession(
        RHIDevice& device,
        GeometryPool& pool,
        const AssetRoots& roots
    )
        : WorldSession(SystemRegistry::Global(), device, pool, roots) {}

    void GameSession::Frame(f64 realSeconds) {
        GetWorld().Frame(realSeconds, paused ? 0.0 : realSeconds * timeScale);
    }
}
