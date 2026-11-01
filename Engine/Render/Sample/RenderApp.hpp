#pragma once

#include "FrameHost.hpp"
#include "Main.hpp"
#include "RenderSession.hpp"

namespace Crowy
{
    class RenderApp: public FrameHost {
    private:
        RAII<RenderSession> ownSession;

    public:
        using FrameHost::FrameHost;

    private:
        RenderSession& openSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        ) override;
    };
}
