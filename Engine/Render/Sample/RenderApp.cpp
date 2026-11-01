#include "RenderApp.hpp"

namespace Crowy
{
    RenderSession& RenderApp::openSession(
        RHIDevice& device,
        GeometryPool& pool,
        const AssetRoots& roots
    ) {
        ownSession = std::make_unique<RenderSession>(device, pool, roots);
        return *ownSession;
    }
}
