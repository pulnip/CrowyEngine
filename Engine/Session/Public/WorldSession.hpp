#pragma once

#include <filesystem>

#include "AssetRoots.hpp"
#include "DOM.hpp"
#include "GeometryPool.hpp"
#include "PrimitiveActors.hpp"
#include "RHIFWD.hpp"
#include "RenderCamera.hpp"
#include "RenderSession.hpp"
#include "Semantics.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    // One World, the RenderSession it draws into and the services it asks
    // for, destroyed together: the World first, whose rows hold asset
    // handles, then the RenderSession. It must die before the device and
    // the pool the RenderSession was made on.
    class WorldSession {
    private:
        RenderSession render;
        CameraSnapshot camera;
        PrimitiveActors primitiveActors;
        World world;

    public:
        ~WorldSession() = default;
        CROWY_DECLARE_PINNED(WorldSession)

        WorldSession(
            const SystemRegistry& systems,
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        );

        // Into the empty World, and one frame of zero time: the actors' rows
        // have landed and the scene holds what the first frame draws.
        void Load(const std::filesystem::path& file);
        void Load(DOM::Value scene);

        auto& GetWorld() noexcept { return world; }
        const auto& GetWorld() const noexcept { return world; }
        auto& Render() noexcept { return render; }
        const auto& Render() const noexcept { return render; }
        // where the scene's camera component extracts to
        const auto& Camera() const noexcept { return camera; }
    };
}
