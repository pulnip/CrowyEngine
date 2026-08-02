#include <algorithm>
#include <array>
#include <numbers>
#include <vector>
#include "AppFramework.hpp"
#include "InputProvider.hpp"
#include "IntMath.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    // 10,000 unit cubes on a sparse 3D lattice (SkyGrid style), all issued
    // by one ExecuteIndirect call. The camera flies around inside:
    // hold RButton to look, WASD to move, Space/Shift for up/down.
    // Cube color is a 3-axis gradient derived from the drawID in the
    // shader, so a broken drawID channel is immediately visible.
    // The args and DrawData buffers are fully rewritten every frame even
    // though the scene is static — that is the pattern a culling pass
    // will later write into.
    class TenThousandCubes: public App{
        using App::App;

        static constexpr RHIPixelFormat DEPTH_FORMAT = RHIPixelFormat::D32_FLOAT;
        // keep in sync with TenThousandCubes.slang
        static constexpr u32 GRID_X = 20, GRID_Y = 20, GRID_Z = 25;
        static constexpr u32 DRAW_COUNT = GRID_X * GRID_Y * GRID_Z;
        static constexpr f32 BLOCK_SPACING = 5.0f;
        static constexpr f32 BLOCK_HALF_SIZE = 0.5f;

        static constexpr f32 FOV_Y = std::numbers::pi_v<f32> / 3;
        static constexpr f32 NEAR_Z = 0.1f, FAR_Z = 300.0f;
        static constexpr f32 MOVE_SPEED = 15.0f;
        static constexpr f32 LOOK_SENSITIVITY = 0.003f;

        static constexpr Color SKY_COLOR{0.55f, 0.78f, 0.95f, 1.0f};

        // mirror of DrawData in TenThousandCubes.slang
        struct DrawData{
            Mat4 world;
            u32 materialIndex = 0;
            u32 objectID = 0;
            // reserved for vertex pulling
            u32 vbIndex = 0;
            u32 _pad0 = 0;
        };
        static_assert(sizeof(DrawData) == 80);

        struct PassData{
            u64 draws;
        };

        struct FrameUniforms{
            Mat4 viewProj;
        };

        RHIGraphicsPipelineStateRAII pso;
        RHITextureRAII depthBuffer;
        RHIDevice* device = nullptr;

        RHIBufferRAII vertices, indices;
        u32 indexCount = 0;

        RHIBufferRAII frameCB;
        RHIBufferRAII drawDataBuffer;
        RHIBufferRAII argsBuffer;

        // CPU staging reused every frame for the full rewrite
        std::vector<DrawData> drawDataScratch;
        std::vector<RHIDrawIndexedArgs> argsScratch;

        // fly camera; the start pose sits between lattice rows looking
        // straight down +Z — the pose the capture check is written against
        Vec3 cameraPos{50.0f, 50.0f, 37.5f};
        f32 cameraYaw = 0.0f, cameraPitch = 0.0f;
        Vec3 moveInput{};
        f32 aspect = 1.0f;

        static constexpr Vec3 BlockPosition(u32 drawID){
            return Vec3{
                static_cast<f32>(drawID % GRID_X),
                static_cast<f32>(floorDiv(drawID, GRID_X) % GRID_Y),
                static_cast<f32>(floorDiv(drawID, GRID_X * GRID_Y))
            } * BLOCK_SPACING;
        }

        void CreateDepthBuffer(RHIDevice& device, u32 width, u32 height){
            depthBuffer = device.CreateTexture(RHITextureCreateDesc{
                .width = width, .height = height,
                .format = DEPTH_FORMAT,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            });
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/TenThousandCubes.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/TenThousandCubes.slang",
                    .entryPoint = "fs_main"
                },
                .depthStencil = RHIDepthStencilState{
                    .format = DEPTH_FORMAT,
                    .depthWriteEnable = true,
                    .depthFunc = RHIComparisonFunc::Less
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1,
                .profile = "sm_6_8"
            });

            CreateDepthBuffer(device, swapchain.GetWidth(), swapchain.GetHeight());
            aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();

            const auto boxMesh = MakeBox(BLOCK_HALF_SIZE);
            vertices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(Vertex) * boxMesh.vertices.size()),
                .usage = RHIBufferUsage::VertexBuffer,
                .access = RHIMemoryAccess::GPUOnly,
                .initialData = boxMesh.vertices.data()
            });
            indices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(u32) * boxMesh.indices.size()),
                .usage = RHIBufferUsage::IndexBuffer,
                .access = RHIMemoryAccess::GPUOnly,
                .initialData = boxMesh.indices.data()
            });
            indexCount = static_cast<u32>(boxMesh.indices.size());

            frameCB = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(FrameUniforms),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite
            });
            drawDataBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(DrawData) * DRAW_COUNT),
                .usage = RHIBufferUsage::ShaderResource,
                .access = RHIMemoryAccess::CPUWrite
            });
            argsBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(RHIDrawIndexedArgs) * DRAW_COUNT),
                .usage = RHIBufferUsage::IndirectArgument,
                .access = RHIMemoryAccess::CPUWrite
            });

            drawDataScratch.resize(DRAW_COUNT);
            argsScratch.resize(DRAW_COUNT);
        }

        void ProcessInput(const InputProvider& input) override{
            if(input.IsKeyDown(MouseButton::RButton)){
                const auto dpos = input.GetMouseDPos();
                cameraYaw += dpos.x * LOOK_SENSITIVITY;
                cameraPitch = std::clamp(
                    cameraPitch + dpos.y * LOOK_SENSITIVITY,
                    -std::numbers::pi_v<f32> / 2 + 0.01f,
                    std::numbers::pi_v<f32> / 2 - 0.01f
                );
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
            if(moveInput.x == 0.0f && moveInput.y == 0.0f && moveInput.z == 0.0f)
                return;

            const auto rot = rotateMat(CameraRotation());
            const auto right = static_cast<Vec3>(rot[0]);
            const auto forward = static_cast<Vec3>(rot[2]);

            const auto direction = normalize(
                right * moveInput.x +
                Vec3{0.0f, moveInput.y, 0.0f} +
                forward * moveInput.z
            );
            cameraPos = cameraPos +
                direction * (MOVE_SPEED * static_cast<f32>(deltaTime));
        }

        void RewriteGPUBuffers(){
            for(u32 i=0; i<DRAW_COUNT; ++i){
                drawDataScratch[i] = DrawData{
                    .world = translateMat(BlockPosition(i)),
                    .objectID = i
                };
                argsScratch[i] = RHIDrawIndexedArgs{
                    .indexCount = indexCount,
                    .baseInstance = i
                };
            }

            drawDataBuffer->Upload(
                drawDataScratch.data(),
                static_cast<u32>(sizeof(DrawData) * DRAW_COUNT)
            );
            argsBuffer->Upload(
                argsScratch.data(),
                static_cast<u32>(sizeof(RHIDrawIndexedArgs) * DRAW_COUNT)
            );
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            RewriteGPUBuffers();

            const auto view = viewMat(cameraPos, CameraRotation());
            const auto proj = perspective(FOV_Y, aspect, NEAR_Z, FAR_Z);
            frameCB->Upload(FrameUniforms{
                .viewProj = proj * view
            });

            cmdList.TransitionBarrier(*depthBuffer, RHIResourceUsage::DepthStencilWrite);

            auto colorAttachment = backBuffer;
            colorAttachment.clearColor = SKY_COLOR;
            std::array colorAttachments = {colorAttachment};
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
                .depthAttachment = RHIDepthAttachment{
                    .texture = depthBuffer.get(),
                    .loadAction = RHILoadAction::Clear,
                    .storeAction = RHIStoreAction::DontCare,
                    .clearDepthStencil = {.depth = 1.0f}
                }
            });
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetVertexBuffer(*vertices, 0, sizeof(Vertex));
            cmdList.SetIndexBuffer(*indices);
            cmdList.SetGraphicsConstantBuffer(*frameCB, 0);
            cmdList.SetPushGraphicsConstants(PassData{
                .draws = drawDataBuffer->GetReadableID(
                    static_cast<u32>(sizeof(DrawData))
                )
            });

            cmdList.ExecuteIndirect(DrawBatch{
                .pso = pso.get(),
                .args = argsBuffer.get(),
                .drawCount = DRAW_COUNT
            });

            cmdList.EndRenderPass();
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
        .title = "TenThousandCubes",
        .width = 800, .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<TenThousandCubes>(windowConfig);
}
