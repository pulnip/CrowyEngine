#include <algorithm>
#include <array>
#include <numbers>
#include <print>
#include <vector>
#include "AppFramework.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "Terrain.hpp"
#include "TerrainMarch.hpp"
#include "TerrainUI.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

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

        static constexpr RHIPixelFormat DEPTH_FORMAT = RHIPixelFormat::D32_FLOAT;

        // the defaults mesh ~99k triangles. This leaves room for the cave
        // sliders without reserving the 1.5M the cell count would permit;
        // past it the marcher drops cells and says so
        static constexpr u32 TRIANGLE_CAPACITY = 600'000;

        static constexpr f32 FOV_Y = std::numbers::pi_v<f32> / 3;
        static constexpr f32 NEAR_Z = 0.1f, FAR_Z = 500.0f;
        static constexpr f32 MOVE_SPEED = 40.0f;
        static constexpr f32 LOOK_SENSITIVITY = 0.003f;

        static constexpr Color SKY_COLOR{0.52f, 0.68f, 0.86f, 1.0f};

        // mirrors ResourceData in Engine/Shader/Terrain.slang
        struct PushConstants{
            u64 vertices;
        };
        static_assert(sizeof(PushConstants) == 8);

        RHIDevice* device = nullptr;

        RHIGraphicsPipelineStateRAII FILL_DEFAULT;
        RHIGraphicsPipelineStateRAII WIRE_DEFAULT;
        RHIGraphicsPipelineStateRAII FILL_NORMAL;
        RHIGraphicsPipelineStateRAII WIRE_NORMAL;

        RHITextureRAII depthBuffer;
        RHIBufferRAII frameCB;

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

        Vec3 cameraPos = TERRAIN_CAMERA_START_POS;
        f32 cameraYaw = TERRAIN_CAMERA_START_YAW;
        f32 cameraPitch = TERRAIN_CAMERA_START_PITCH;
        Vec3 moveInput{};
        f32 aspect = 1.0f;

        void CreateDepthBuffer(RHIDevice& device, u32 width, u32 height){
            depthBuffer = device.CreateTexture(RHITextureCreateDesc{
                .width = width, .height = height,
                .format = DEPTH_FORMAT,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            });
        }

        static RHIGraphicsPipelineStateDesc PipelineDesc(
            RHIPixelFormat colorFormat,
            RHIFillMode fillMode,
            bool debugNormal
        ){
            return RHIGraphicsPipelineStateDesc{
                // no vertex layout: vertices pulled by SV_VertexID
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/Terrain.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .fillMode = fillMode,
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/Terrain.slang",
                    .entryPoint = debugNormal ? "fs_debug_normal" : "fs_main"
                },
                .depthStencil = RHIDepthStencilState{
                    .format = DEPTH_FORMAT,
                    .depthWriteEnable = true,
                    .depthFunc = RHIComparisonFunc::Less
                },
                .renderTargetFormats = {colorFormat},
                .renderTargetCount = 1
            };
        }

        RHIGraphicsPipelineState& SurfacePipeline(){
            return ctx.debug.showNormals ?
                (ctx.debug.wireframe ? *WIRE_NORMAL : *FILL_NORMAL) :
                (ctx.debug.wireframe ? *WIRE_DEFAULT : *FILL_DEFAULT);
        }

        // The pacer waits for frame index F - RHI_FRAMES_IN_FLIGHT + 1 before
        // recording frame F, so a copy recorded while the index read `at` has
        // certainly landed once the index reaches at + RHI_FRAMES_IN_FLIGHT.
        // Reading then costs no wait, and the stats trail by a few frames.
        void CollectCounter(){
            if(!counterCopyInFlight)
                return;
            if(device->GetFrameIndexRef() < counterCopyFrame + RHI_FRAMES_IN_FLIGHT)
                return;

            counterReadback->Download(&counter, sizeof(counter));
            counterCopyInFlight = false;

            ctx.stats.triangleCount = counter.DrawableTriangles();
            ctx.stats.vertexCount = ctx.stats.triangleCount * 3;

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

        std::vector<Widget> MakeSampleStats(){
            return {
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

            FILL_DEFAULT = device.CreatePipelineState(
                PipelineDesc(swapchain.GetFormat(), RHIFillMode::Solid, false)
            );
            WIRE_DEFAULT = device.CreatePipelineState(
                PipelineDesc(swapchain.GetFormat(), RHIFillMode::Wireframe, false)
            );
            FILL_NORMAL = device.CreatePipelineState(
                PipelineDesc(swapchain.GetFormat(), RHIFillMode::Solid, true)
            );
            WIRE_NORMAL = device.CreatePipelineState(
                PipelineDesc(swapchain.GetFormat(), RHIFillMode::Wireframe, true)
            );

            CreateDepthBuffer(device, swapchain.GetWidth(), swapchain.GetHeight());
            aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();

            frameCB = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(TerrainFrameUniforms),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite
            }, "TerrainFrameCB");

            marcher = std::make_unique<TerrainMarcher>(device, TRIANGLE_CAPACITY);
            counterReadback = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(TerrainMarchCounter),
                .usage = RHIBufferUsage::CopyDst,
                .access = RHIMemoryAccess::CPURead
            }, "TerrainMarchCounterReadback");

            uiRenderer = std::make_unique<UIRenderer>(
                device, swapchain.GetFormat(), DEPTH_FORMAT
            );
            // re-marching is cheap enough to chase a slider with
            ctx.autoRebuild = true;
            panel = std::make_unique<TerrainPanel>(ctx, MakeSampleStats());
        }

        void ProcessInput(const InputProvider& input) override{
            const auto& io = ImGui::GetIO();

            if(input.IsKeyDown(MouseButton::RButton) && !io.WantCaptureMouse){
                const auto dpos = input.GetMouseDPos();
                cameraYaw += dpos.x * LOOK_SENSITIVITY;
                cameraPitch = std::clamp(
                    cameraPitch + dpos.y * LOOK_SENSITIVITY,
                    -std::numbers::pi_v<f32> / 2 + 0.01f,
                    std::numbers::pi_v<f32> / 2 - 0.01f
                );
            }

            // otherwise typing a seed into the panel flies the camera away
            if(io.WantCaptureKeyboard){
                moveInput = Vec3{};
                return;
            }

            moveInput = Vec3{
                (input.IsKeyDown(KeyCode::D) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::A) ? 1.0f : 0.0f),
                (input.IsKeyDown(KeyCode::Space) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Shift) ? 1.0f : 0.0f),
                (input.IsKeyDown(KeyCode::W) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::S) ? 1.0f : 0.0f)
            };
        }

        Vec4 CameraRotation() const{
            return quat(rotateY(cameraYaw), rotateX(cameraPitch));
        }

        void OnUpdate(f64 deltaTime, f64) override{
            ctx.stats.frameTimeMs = deltaTime * 1000.0;

            CollectCounter();

            if(moveInput.x != 0.0f || moveInput.y != 0.0f || moveInput.z != 0.0f){
                const auto rot = rotateMat(CameraRotation());
                const auto right = static_cast<Vec3>(rot[0]);
                const auto forward = static_cast<Vec3>(rot[2]);

                const auto direction = normalize(
                    right * moveInput.x +
                    unitY() * moveInput.y +
                    forward * moveInput.z
                );
                cameraPos += direction * (MOVE_SPEED * static_cast<f32>(deltaTime));
            }

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

                const auto edges = marcher->Record(cmdList, ctx.params, {
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
                    counterCopyFrame = device->GetFrameIndexRef();
                }

                // the draw reads what the marching pass just wrote; a frame
                // that skips the rebuild reads the same bytes again and needs
                // no barrier at all
                bufferAcquires = {edges.vertices, edges.args};
                rebuildPending = false;
            }

            const auto view = viewMat(cameraPos, CameraRotation());
            const auto proj = perspective(FOV_Y, aspect, NEAR_Z, FAR_Z);
            frameCB->Upload(TerrainFrameUniforms{
                .viewProj = proj * view,
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

            cmdList.SetGraphicsConstantBuffer(*frameCB, 0);
            cmdList.SetPushGraphicsConstants(PushConstants{
                .vertices = marcher->Vertices().GetReadableID(
                    static_cast<u32>(sizeof(TerrainVertex))
                )
            });
            // how many vertices this draws is only known to the GPU
            cmdList.ExecuteIndirect(DrawBatch{
                .pso = &SurfacePipeline(),
                .args = &marcher->Args(),
                .drawCount = 1
            });

            uiRenderer->Record(cmdList);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }

        void OnResize(u32 width, u32 height) override{
            CreateDepthBuffer(*device, width, height);
            aspect = static_cast<f32>(width) / height;
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
