#include <algorithm>
#include <array>
#include <chrono>
#include <numbers>
#include <print>
#include <vector>
#include "AppFramework.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "Terrain.hpp"
#include "TerrainSpan.hpp"
#include "TerrainUI.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

namespace Crowy
{
    // Terrain from per-column solid spans, meshed on the CPU.
    // Deliberately faceted - the smooth reading of the same field is MarchingTerrainSample.
    class SpanTerrainSample: public App{
        using App::App;

        static constexpr RHIPixelFormat DEPTH_FORMAT = RHIPixelFormat::D32_FLOAT;

        // measured: defaults mesh ~0.59M vertices, the busiest slider corner ~1.40M.
        // Allocated once and never grown (no deferred free in the RHI);
        // overflow drops the rest instead.
        static constexpr u32 VERTEX_CAPACITY = 1'500'000;

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
        RHIBufferRAII vertexBuffer;

        RAII<UIRenderer> uiRenderer = nullptr;
        UIContext ctx;
        RAII<TerrainPanel> panel;

        TerrainSpanField field;
        std::vector<TerrainVertex> mesh;
        TerrainSpanMeshOptions meshOptions;
        TerrainSpanMeshStats meshStats;

        // Notice. a CPUWrite buffer keeps one slot per frame in flight, so a
        // mesh built once must be uploaded once per slot
        u32 pendingUploads = 0;

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

        void Rebuild(){
            using clock = std::chrono::steady_clock;
            const auto start = clock::now();

            field = ExtractTerrainSpans(ctx.params);
            meshStats = BuildTerrainSpanMesh(
                field, mesh, VERTEX_CAPACITY, meshOptions
            );

            ctx.stats.buildTimeMs = std::chrono::duration<f64, std::milli>(
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
            vertexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = VERTEX_CAPACITY * static_cast<u32>(sizeof(TerrainVertex)),
                .usage = RHIBufferUsage::ShaderResource,
                .access = RHIMemoryAccess::CPUWrite
            }, "TerrainVertices");

            mesh.reserve(VERTEX_CAPACITY);

            uiRenderer = std::make_unique<UIRenderer>(
                device, swapchain.GetFormat(), DEPTH_FORMAT
            );
            // a CPU rebuild is tens of ms - too slow to chase a slider
            ctx.autoRebuild = false;
            panel = std::make_unique<TerrainPanel>(ctx, MakeSampleStats());

            Rebuild();
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
                cmdList.SetPipelineState(
                    ctx.debug.showNormals ?
                        (ctx.debug.wireframe ? *WIRE_NORMAL : *FILL_NORMAL) :
                        (ctx.debug.wireframe ? *WIRE_DEFAULT : *FILL_DEFAULT)
                );
                cmdList.SetGraphicsConstantBuffer(*frameCB, 0);
                cmdList.SetPushGraphicsConstants(PushConstants{
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
            aspect = static_cast<f32>(width) / height;
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
