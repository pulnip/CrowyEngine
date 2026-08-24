#include <array>
#include <print>
#include <vector>
#include "AppFramework.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "Terrain.hpp"
#include "TerrainCamera.hpp"
#include "TerrainMarch.hpp"
#include "TerrainSurface.hpp"
#include "TerrainUI.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    // Terrain from marching cubes over the density field, meshed on the GPU.
    // The smooth reading of the same field SpanTerrainSample facets.
    //
    // How many triangles came out is never known to the CPU: a compute pass
    // writes the draw arguments and ExecuteIndirect consumes them. The only
    // thing that comes back is the counter, a few frames late, to drive the
    // stats and the overflow warning.
    class MarchingTerrainSample: public App{
        using App::App;

        // the defaults mesh ~99k triangles. This leaves room for the cave
        // sliders without reserving the 1.5M the cell count would permit;
        // past it the marcher drops cells and says so
        static constexpr u32 TRIANGLE_CAPACITY = 600'000;

        static constexpr Color SKY_COLOR{0.52f, 0.68f, 0.86f, 1.0f};

        RHIDevice* device = nullptr;

        RAII<TerrainSurface> surface;
        RHITextureRAII depthBuffer;
        RHIBufferSlice frameCB;

        RAII<TerrainMarcher> marcher;
        RHIBufferRAII counterReadback;

        RAII<UIRenderer> uiRenderer = nullptr;
        UIContext ctx;
        RAII<TerrainPanel> panel;

        // nothing can be drawn until the first march has run
        bool rebuildPending = true;
        // the counter copy is in flight; issuing another would race the read
        bool counterCopyInFlight = false;
        u64 counterCopyFrame = 0;
        TerrainMarchCounter counter{};
        bool overflowReported = false;
        // what the panel asks for, and what the mesh in the buffers actually
        // is - they differ between a toggle and the rebuild that follows it
        TerrainMarchMode mode = TerrainMarchMode::Soup;
        TerrainMarchMode drawnMode = TerrainMarchMode::Soup;

        TerrainCamera camera;

        void CreateDepthBuffer(RHIDevice& device, u32 width, u32 height){
            depthBuffer = device.CreateTexture(RHITextureCreateDesc{
                .width = width, .height = height,
                .format = TERRAIN_DEPTH_FORMAT,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            });
        }

        // The copy rides the batch tagged counterCopyFrame, so the bytes are
        // there the moment the GPU reports that value complete. Reading then
        // costs no wait, and the stats trail by a few frames.
        void CollectCounter(){
            if(!counterCopyInFlight)
                return;
            if(device->GetCompletedFrame() < counterCopyFrame)
                return;

            counterReadback->Download(&counter, sizeof(counter));
            counterCopyInFlight = false;

            ctx.stats.triangleCount = counter.DrawableTriangles();
            // the soup spends three vertices on every triangle; welding spends
            // one per crossing edge and says so in the counter
            ctx.stats.vertexCount = mode == TerrainMarchMode::Soup ?
                ctx.stats.triangleCount * 3 :
                counter.vertexCount;

            // once per episode, not once per rebuild - dragging a slider
            // through an overflowing range re-marches every frame
            if(counter.overflowed == 0){
                overflowReported = false;
            }
            else if(!overflowReported){
                std::println(
                    "MarchingTerrain: triangle capacity {} exceeded "
                    "({} reserved), terrain truncated. "
                    "Raise caveThreshold or lower caveFreq.",
                    TRIANGLE_CAPACITY, counter.triangleCount
                );
                overflowReported = true;
            }
        }

        // bytes the mesh occupies as drawn, so the welding toggle shows its
        // work rather than being taken on faith
        u32 MeshBytes() const{
            const auto vertexBytes =
                ctx.stats.vertexCount * static_cast<u32>(sizeof(TerrainVertex));
            if(mode == TerrainMarchMode::Soup)
                return vertexBytes;

            return vertexBytes +
                ctx.stats.triangleCount * 3 * static_cast<u32>(sizeof(u32));
        }

        std::vector<Widget> MakeSampleStats(){
            return {
                Checkbox{
                    .label = "weld vertices",
                    .onChanged = [this](UIContext& c, bool v){
                        mode = v ?
                            TerrainMarchMode::Welded :
                            TerrainMarchMode::Soup;
                        c.paramsDirty = true;
                    },
                    .v = mode == TerrainMarchMode::Welded
                },
                FloatField{
                    .label = "mesh (MB)",
                    .get = [this]{ return MeshBytes() / (1024.0f * 1024.0f); }
                },
                FloatField{
                    // reservations, not written triangles, so this runs past
                    // 100 to show by how much the parameters overshot
                    .label = "capacity used (%)",
                    .get = [this]{
                        return 100.0f * counter.triangleCount / TRIANGLE_CAPACITY;
                    }
                },
                IntField{
                    .label = "overflowed",
                    .get = [this]{ return counter.overflowed != 0 ? 1 : 0; }
                }
            };
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            surface = std::make_unique<TerrainSurface>(device, swapchain.GetFormat());

            CreateDepthBuffer(device, swapchain.GetWidth(), swapchain.GetHeight());
            camera.SetViewport(swapchain.GetWidth(), swapchain.GetHeight());

            marcher = std::make_unique<TerrainMarcher>(device, TRIANGLE_CAPACITY);
            counterReadback = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(TerrainMarchCounter),
                .usage = RHIBufferUsage::CopyDst,
                .location = RHIMemoryLocation::Readback,
                .cpuAccess = RHICpuAccess::Read
            }, "TerrainMarchCounterReadback");

            uiRenderer = std::make_unique<UIRenderer>(
                device, swapchain.GetFormat(), TERRAIN_DEPTH_FORMAT
            );
            // re-marching is cheap enough to chase a slider with
            ctx.autoRebuild = true;
            panel = std::make_unique<TerrainPanel>(ctx, MakeSampleStats());
        }

        void ProcessInput(const InputProvider& input) override{
            camera.ProcessInput(input);
        }

        void OnUpdate(f64 deltaTime, f64) override{
            ctx.stats.frameTimeMs = deltaTime * 1000.0;

            CollectCounter();
            camera.Update(deltaTime);

            // panel callbacks run during Prepare; a press lands here next frame
            if(ctx.ShouldRebuild()){
                ctx.ClearRebuild();
                rebuildPending = true;
            }
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            std::vector<RHIBufferBarrier> bufferAcquires;

            if(rebuildPending){
                // only ask for the counter when the last one has been read,
                // so the copy never overtakes the CPU still reading it
                const bool copyCounter = !counterCopyInFlight;

                // the mode is latched here so the pass, the draw and the
                // stats all speak about the same recording
                drawnMode = mode;

                const auto edges = marcher->Record(cmdList, ctx.params, drawnMode, {
                    .counter = copyCounter ?
                        RHIResourceUsage::CopySrc :
                        RHIResourceUsage::StorageCompute
                });

                if(copyCounter){
                    const std::array acquires{edges.counter};
                    cmdList.BeginBlitPass({}, acquires);
                    cmdList.Copy(
                        marcher->Counter(), *counterReadback,
                        0, 0, sizeof(TerrainMarchCounter)
                    );
                    cmdList.EndBlitPass();

                    counterCopyInFlight = true;
                    // this recording goes out with the next submit
                    counterCopyFrame = device->GetSubmittedFrame() + 1;
                }

                // the draw reads what the marching pass just wrote; a frame
                // that skips the rebuild reads the same bytes again and needs
                // no barrier at all
                bufferAcquires = {edges.vertices, edges.args};
                if(drawnMode == TerrainMarchMode::Welded)
                    bufferAcquires.push_back(edges.indices);

                rebuildPending = false;
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

            std::vector<RHITextureBarrier> textureAcquires{
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
            textureAcquires.append_range(uiRenderer->TextureAcquires());

            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
                .depthAttachment = RHIDepthAttachment{
                    .texture = depthBuffer.get(),
                    .loadAction = RHILoadAction::Clear,
                    .storeAction = RHIStoreAction::DontCare,
                    .clearDepthStencil = {.depth = 1.0f}
                }
            }, textureAcquires, bufferAcquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetGraphicsConstantBuffer(frameCB, 0);
            cmdList.SetPushGraphicsConstants(TerrainSurfacePush{
                .vertices = marcher->Vertices().GetReadableID(
                    static_cast<u32>(sizeof(TerrainVertex))
                )
            });
            // how much this draws is only ever known to the GPU
            auto& pipeline = surface->Pipeline(ctx.debug);
            auto& args = marcher->Args(drawnMode);
            if(drawnMode == TerrainMarchMode::Welded){
                cmdList.ExecuteIndirectIndexed(DrawBatchIndexed{
                    .pso = &pipeline,
                    .args = &args,
                    .drawCount = 1,
                    .indices = RHIIndexBufferView{
                        .buffer = &marcher->Indices()
                    }
                });
            }
            else{
                // the soup path emits its triangles unindexed
                cmdList.ExecuteIndirect(DrawBatch{
                    .pso = &pipeline,
                    .args = &args,
                    .drawCount = 1
                });
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
        .title = "MarchingTerrainSample",
        .width = 1280, .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<MarchingTerrainSample>(windowConfig);
}
