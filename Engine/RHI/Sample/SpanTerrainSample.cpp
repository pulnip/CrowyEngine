#include <array>
#include <chrono>
#include <print>
#include <vector>
#include "AppFramework.hpp"
#include "RHIBuffer.hpp"
#include "Terrain.hpp"
#include "TerrainCamera.hpp"
#include "TerrainSpan.hpp"
#include "TerrainSurface.hpp"
#include "TerrainUI.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    // Terrain from per-column solid spans, meshed on the CPU.
    // Deliberately faceted - the smooth reading of the same field is MarchingTerrainSample.
    class SpanTerrainSample: public App{
        using App::App;

        // measured: defaults mesh ~0.59M vertices, the busiest slider corner ~1.40M.
        // Allocated once and never grown (no deferred free in the RHI);
        // overflow drops the rest instead.
        static constexpr u32 VERTEX_CAPACITY = 1'500'000;

        static constexpr Color SKY_COLOR{0.52f, 0.68f, 0.86f, 1.0f};

        RHIDevice* device = nullptr;

        RAII<TerrainSurface> surface;
        RHITextureRAII depthBuffer;

        RHIBufferSlice frameCB;
        RHIBufferRAII vertexBuffer;

        RAII<UIRenderer> uiRenderer = nullptr;
        UIContext ctx;
        RAII<TerrainPanel> panel;

        TerrainSpanField field;
        std::vector<TerrainVertex> mesh;
        TerrainSpanMeshOptions meshOptions;
        TerrainSpanMeshStats meshStats;
        f64 buildTimeMs = 0.0;

        // Notice. a CPUWrite buffer keeps one slot per frame in flight, so a
        // mesh built once must be uploaded once per slot
        u32 pendingUploads = 0;

        TerrainCamera camera;

        void CreateDepthBuffer(RHIDevice& device, u32 width, u32 height){
            depthBuffer = device.CreateTexture(RHITextureCreateDesc{
                .width = width, .height = height,
                .format = TERRAIN_DEPTH_FORMAT,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            });
        }

        void Rebuild(){
            using clock = std::chrono::steady_clock;
            const auto start = clock::now();

            field = ExtractTerrainSpans(ctx.params);
            meshStats = BuildTerrainSpanMesh(
                field, mesh, VERTEX_CAPACITY, meshOptions
            );

            buildTimeMs = std::chrono::duration<f64, std::milli>(
                clock::now() - start
            ).count();
            ctx.stats.vertexCount = meshStats.vertexCount;
            ctx.stats.triangleCount = meshStats.vertexCount / 3;

            if(meshStats.overflowed){
                std::println(
                    "SpanTerrain: vertex capacity {} exceeded, terrain truncated. "
                    "Lower caveFreq or raise caveThreshold.",
                    VERTEX_CAPACITY
                );
            }

            pendingUploads = RHI_FRAMES_IN_FLIGHT;
        }

        std::vector<Widget> MakeSampleStats(){
            return {
                FloatField{
                    // the mesher is the CPU's whole job here, so this is the
                    // number the auto-rebuild checkbox lives or dies by
                    .label = "build (ms)",
                    .get = [this]{ return static_cast<f32>(buildTimeMs); }
                },
                IntField{
                    .label = "columns",
                    .get = [this]{
                        return static_cast<int>(meshStats.columnCount);
                    }
                },
                IntField{
                    .label = "spans",
                    .get = [this]{
                        return static_cast<int>(meshStats.spanCount);
                    }
                },
                IntField{
                    .label = "max spans / column",
                    .get = [this]{
                        return static_cast<int>(meshStats.maxSpansPerColumn);
                    }
                },
                FloatField{
                    // reads 100 exactly when the mesh was truncated
                    .label = "capacity used (%)",
                    .get = [this]{
                        return 100.0f * meshStats.vertexCount / VERTEX_CAPACITY;
                    }
                },
                Checkbox{
                    .label = "vertical walls",
                    .onChanged = [this](UIContext& c, bool v){
                        meshOptions.emitWalls = v;
                        c.paramsDirty = true;
                    },
                    .v = meshOptions.emitWalls
                }
            };
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            surface = std::make_unique<TerrainSurface>(device, swapchain.GetFormat());

            CreateDepthBuffer(device, swapchain.GetWidth(), swapchain.GetHeight());
            camera.SetViewport(swapchain.GetWidth(), swapchain.GetHeight());
            vertexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = VERTEX_CAPACITY * static_cast<u32>(sizeof(TerrainVertex)),
                .usage = RHIBufferUsage::ShaderResource,
                .location = RHIMemoryLocation::Upload,
                .cpuAccess = RHICpuAccess::Write
            }, "TerrainVertices");

            mesh.reserve(VERTEX_CAPACITY);

            uiRenderer = std::make_unique<UIRenderer>(
                device, swapchain.GetFormat(), TERRAIN_DEPTH_FORMAT
            );
            // a CPU rebuild is tens of ms - too slow to chase a slider
            ctx.autoRebuild = false;
            panel = std::make_unique<TerrainPanel>(ctx, MakeSampleStats());

            Rebuild();
        }

        void ProcessInput(const InputProvider& input) override{
            camera.ProcessInput(input);
        }

        void OnUpdate(f64 deltaTime, f64) override{
            ctx.stats.frameTimeMs = deltaTime * 1000.0;

            camera.Update(deltaTime);

            // panel callbacks run during Prepare; a press lands here next frame
            if(ctx.ShouldRebuild()){
                ctx.ClearRebuild();
                Rebuild();
            }
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            const auto vertexCount = meshStats.vertexCount;
            const auto vertexBytes =
                vertexCount * static_cast<u32>(sizeof(TerrainVertex));

            if(pendingUploads > 0 && vertexBytes > 0){
                vertexBuffer->Upload(mesh.data(), vertexBytes);
                --pendingUploads;
            }

            frameCB = Device().UploadTransient(TerrainFrameUniforms{
                .viewProj = camera.ViewProj(),
                .toLight = {0.38f, 0.82f, 0.43f},
                .ambient = 0.28f
            });

            uiRenderer->Prepare(cmdList, panel->Get(ctx), ctx);

            auto colorAttachment = backBuffer;
            colorAttachment.clearColor = SKY_COLOR;
            std::array colorAttachments = {colorAttachment};

            std::vector<RHITextureBarrier> acquires{
                AcquireBackBuffer(backBuffer),
                // WAR with the previous frame's depth; the pass clears anyway
                MakeCrossSubmissionBarrier(
                    *depthBuffer,
                    RHIResourceUsage::DepthWrite,
                    RHIResourceUsage::DepthWrite,
                    /*discardContents=*/true
                )
            };
            // the pass samples the font atlas Prepare just refreshed
            acquires.append_range(uiRenderer->TextureAcquires());

            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
                .depthAttachment = RHIDepthAttachment{
                    .texture = depthBuffer.get(),
                    .loadAction = RHILoadAction::Clear,
                    .storeAction = RHIStoreAction::DontCare,
                    .clearDepthStencil = {.depth = 1.0f}
                }
            }, acquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            if(vertexCount > 0){
                cmdList.SetPipelineState(surface->Pipeline(ctx.debug));
                cmdList.SetGraphicsConstantBuffer(frameCB, 0);
                cmdList.SetPushGraphicsConstants(TerrainSurfacePush{
                    .vertices = vertexBuffer->GetReadableID(
                        static_cast<u32>(sizeof(TerrainVertex))
                    )
                });
                cmdList.Draw(vertexCount);
            }

            uiRenderer->Record(cmdList);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }

        void OnResize(u32 width, u32 height) override{
            CreateDepthBuffer(*device, width, height);
            camera.SetViewport(width, height);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "SpanTerrainSample",
        .width = 1280, .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<SpanTerrainSample>(windowConfig);
}
