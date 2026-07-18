#include <algorithm>
#include <array>
#include <numbers>
#include "AppFramework.hpp"
#include "ImageLoader.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace{
    struct Vertex{
        Crowy::Vec3 position;
        Crowy::Vec3 normal;
    };

    inline constexpr std::array VERTEX_INPUT_LAYOUT = {
        Crowy::RHIVertexElement{
            .semanticName = "POSITION",
            .semanticIndex = 0,
            .format = Crowy::RHIPixelFormat::RGB32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 0,
            .classification = Crowy::RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        Crowy::RHIVertexElement{
            .semanticName = "NORMAL",
            .semanticIndex = 0,
            .format = Crowy::RHIPixelFormat::RGB32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 12,
            .classification = Crowy::RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        }
    };

    struct MeshData{
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };
    MeshData makeSphere(
        float radius = 1.0f,
        int slices = 32,
        int stacks = 16
    ){
        constexpr float pi = std::numbers::pi_v<float>;
        const float dTheta = 2.0f * pi / slices;
        const float dPhi = pi / stacks;

        std::vector<Vertex> vertices;
        vertices.reserve((slices+1) * (stacks));

        for(int i=0; i<=stacks; ++i){
            const float phi = dPhi * i;
            const float y = radius * std::cosf(phi);
            const float rad = radius * std::sinf(phi);

            // const float v = static_cast<float>(i) / stacks;

            for(int j=0; j<=slices; ++j){
                const float theta = dTheta * j;
                const float cost = std::cosf(theta);
                const float sint = std::sinf(theta);

                const float x = rad * cost;
                const float z = rad * sint;
                // const float u = static_cast<float>(j) / slices;

                // Vec2 texCoord{u, v};
                // Vec3 tangent{-sint, 0.0f, cost};

                vertices.push_back(Vertex{
                    .position = Crowy::Vec3{x, y, z},
                    .normal = Crowy::Vec3{x, y, z}
                });
            }
        }

        std::vector<std::uint32_t> indices;
        indices.reserve(6 * slices * stacks);

        for(int i=0; i<stacks; ++i){
            const int base = (slices+1) * i;

            for(int j=0; j<slices; ++j){
                const int topLeft = base + j;
                const int topRight = base + (j + 1);
                const int bottomLeft = base + (slices + 1) + j;
                const int bottomRight = base + (slices + 1) + (j + 1);

                indices.push_back(topLeft);
                indices.push_back(topRight);
                indices.push_back(bottomRight);

                indices.push_back(topLeft);
                indices.push_back(bottomRight);
                indices.push_back(bottomLeft);
            }
        }

        return MeshData{
            .vertices = std::move(vertices),
            .indices = std::move(indices)
        };
    }
}

namespace Crowy
{
    class CubeMapping: public App{
    private:
        RHIGraphicsPipelineStateRAII pso;

        RHIBufferRAII vertices, indices;
        u32 indexCount = 0;
        RHITextureRAII cubeMap;

        struct PushConstants{
            u64 texture;
        };

        // camera geometry
        f32 yaw = 0.0f, pitch = 0.0f;
        Vec4 GetCameraRot() const noexcept{
            // yaw in world, then pitch in camera-local frame
            return quat(rotateY(yaw), rotateX(pitch));
        }

        // camera lens
        static constexpr f32 fovY = static_cast<f32>(toRadian(60.0));
        f32 aspect;
        static constexpr f32 nearZ = 0.1f, farZ = 100.0f;

        struct Uniforms{
            Mat4 viewProj = unitMat();
        };
        RHIBufferRAII uniformsCB;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = ::VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/CubeMapping.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/CubeMapping.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });

            auto cubeMesh = ::makeSphere(10.0f);
            vertices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(Vertex) * cubeMesh.vertices.size()),
                .usage = RHIBufferUsage::VertexBuffer,
                .access = RHIMemoryAccess::GPUOnly,
                .initialData = cubeMesh.vertices.data()
            });
            std::reverse(cubeMesh.indices.begin(), cubeMesh.indices.end());
            indices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(u32) * cubeMesh.indices.size()),
                .usage = RHIBufferUsage::IndexBuffer,
                .access = RHIMemoryAccess::GPUOnly,
                .initialData = cubeMesh.indices.data()
            });
            indexCount = static_cast<u32>(cubeMesh.indices.size());

            auto right = LoadImage("Content/Assets/skybox/right.ktx2");
            auto left = LoadImage("Content/Assets/skybox/left.ktx2");
            auto top = LoadImage("Content/Assets/skybox/top.ktx2");
            auto bottom = LoadImage("Content/Assets/skybox/bottom.ktx2");
            auto front = LoadImage("Content/Assets/skybox/front.ktx2");
            auto back = LoadImage("Content/Assets/skybox/back.ktx2");

            std::vector<RHISubresourceData> subs;
            subs.reserve(6 * back.subs.size());
            subs.append_range(right.subs);
            subs.append_range(left.subs);
            subs.append_range(top.subs);
            subs.append_range(bottom.subs);
            subs.append_range(front.subs);
            subs.append_range(back.subs);

            cubeMap = device.CreateTexture(RHITextureCreateDesc{
                .width = back.width, .height = back.height,
                .mipLevels = back.mipLevels,
                .arraySize = 6,
                .format = back.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = subs
            });

            aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();
            uniformsCB = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(Uniforms),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite
            });
        }

        void ProcessInput(const InputProvider& input) override{
            if(!input.IsKeyDown(MouseButton::RButton))
                return;

            // radian per pixel
            constexpr f32 SENSITIVITY = 0.003f;
            // do not let the camera flip over the poles
            constexpr f32 PITCH_LIMIT = 0.5f * std::numbers::pi_v<f32> - 0.01f;

            const auto dpos = input.GetMouseDPos();

            yaw += dpos.x * SENSITIVITY;
            pitch = std::clamp(
                pitch + dpos.y * SENSITIVITY,
                -PITCH_LIMIT, PITCH_LIMIT
            );
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            const auto view = viewMat(zeros(), GetCameraRot());
            const auto proj = perspective(fovY, aspect, nearZ, farZ);

            Uniforms uniforms{
                .viewProj = proj * view
            };
            uniformsCB->Upload(uniforms);

            std::array colorAttachments = {
                RHIColorAttachment{
                    .texture = backBuffer.texture,
                    .loadAction = backBuffer.loadAction,
                    .storeAction = backBuffer.storeAction,
                    .clearColor = Colors::Grey
                }
            };
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            });
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            cmdList.SetVertexBuffer(
                *vertices,
                0,
                sizeof(Vertex)
            );
            cmdList.SetIndexBuffer(
                *indices,
                RHIIndexFormat::UInt32
            );

            cmdList.SetPushGraphicsConstants(PushConstants{
                .texture = cubeMap->GetReadableID(RHITextureViewDesc{
                    .format = cubeMap->GetFormat(),
                    .config = RHITextureViewDesc::TexCube{}
                })
            });
            cmdList.SetGraphicsConstantBuffer(
                *uniformsCB,
                0
            );
            cmdList.DrawIndexed(indexCount);

            cmdList.EndRenderPass();
        }

        void OnResize(u32 width, u32 height) override{
            aspect = static_cast<f32>(width) / height;
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "CubeMapping",
        .width = 800, .height = 800,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<CubeMapping>(windowConfig);
}
