#pragma once

#include <memory>

#include "AppFramework.hpp"
#include "FlyCamera.hpp"
#include "GeometryPool.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

namespace Crowy
{
    using GeometryPoolPtr = RAII<GeometryPool>;
    using SceneRendererPtr = RAII<SceneRenderer>;

    // Sample framework for Engine/Render: Owns the frame order and seals it
    class RenderApp: public App {
    public:
        // the sample's statement of what its capture is taken against
        struct Config {
            RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
            Color clearColor = Colors::Black;

            // worst case, not the visible count
            u32 drawCapacity = 4096;
            u32 viewCount = 1;

            // element counts, as GeometryPool takes them
            u32 vertexPoolCapacity = 1024;
            u32 indexPoolCapacity = 4096;

            FlyCamera::Config camera{};
        };

        static constexpr u32 ViewMain = 0;
        static constexpr u32 ViewCBSlot = 0;

    private:
        Config config;

        RHIDevice* device = nullptr;
        RHITextureRAII depthBuffer;
        f32 aspect = 1.0f;

        GeometryPoolPtr geometryPool;
        SceneRendererPtr renderer;
        RenderScene scene;
        FlyCamera camera;

        bool reportedCullStats = false;

    public:
        ~RenderApp() override;
        CROWY_DECLARE_PINNED(RenderApp)

        explicit RenderApp(const Config& config);

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override final;
        void OnInitialRecord(RHICommandList& cmdList) override final;
        void ProcessInput(const InputProvider& input) override final;
        void OnUpdate(f64 deltaTime, f64 elapsedTime) override final;
        void OnRecord(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override final;
        void OnResize(u32 width, u32 height) override final;

    protected:
        virtual void OnCreatePipelines(
            RHIDevice& device,
            RHIPixelFormat colorFormat,
            RHIPixelFormat depthFormat
        ) = 0;

        // inside the geometry pool's blit pass, which the framework opens
        virtual void OnBuildGeometry(
            RHICommandList& cmdList,
            GeometryPool& pool
        ) = 0;

        // The only sample code allowed to see both where a thing is in the
        // world and what the renderer stores about it.
        virtual void ExtractScene(RenderScene& scene) = 0;

        virtual RHIGraphicsPipelineState& Pipeline() = 0;

        // Override to push a struct starting with the same members
        // when a shader wants more root constants.
        virtual void OnBindPass(RHICommandList& cmdList, u64 drawDataID);

        RHIDevice& Device() noexcept { return *device; }
        GeometryPool& Geometry() noexcept { return *geometryPool; }
        RenderScene& Scene() noexcept { return scene; }
        SceneRenderer& Renderer() noexcept { return *renderer; }
        const FlyCamera& Camera() const noexcept { return camera; }
        f32 Aspect() const noexcept { return aspect; }

    private:
        void createDepthBuffer(u32 width, u32 height);
        void reportCullStatsOnce();
    };
}
