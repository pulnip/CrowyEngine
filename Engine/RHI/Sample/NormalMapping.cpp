#include <array>
#include "AppFramework.hpp"
#include "ImageLoader.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class NormalMapping: public App{
    private:
        RHIGraphicsPipelineStateRAII pso;

        RHIBufferRAII vertices, indices;
        u32 indexCount = 0;
        RHITextureRAII diffuse, normal, bump;

        struct PushConstants{
            u64 diffuse;
            u64 normal;
            u64 bump;
        };

        // model geometry
        Vec4 boxRot = unitQuat();

        // environment
        const Vec3 toLight = normalize(Vec3{0.4f, 0.7f, -0.6f});

        // camera geometry
        static constexpr Vec3 eye{0.0f, 0.0f, -4.0f};

        // camera lens
        static constexpr f32 fovY = static_cast<f32>(toRadian(60.0));
        f32 aspect = 1.0f;
        static constexpr f32 nearZ = 0.1f, farZ = 100.0f;

        struct Uniforms{
            Mat4 model = unitMat();
            Mat4 viewProj = unitMat();
            Vec3 cameraPos{};
            float parallaxScale = 0.04f;
            Vec3 toLight{};
            float specularPower = 64.0f;
            float ambient = 0.15f;
        };
        RHIBufferRAII uniformsCB;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/NormalMapping.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/NormalMapping.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });

            auto boxMesh = MakeBox(1.0f);
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

            // the normal map and the height map describe the same surface at different smoothness;
            // - the normal map is a wide, gentle bevel per plank,
            // - the height map a narrow, sharp notch that also carries the frame and the brace.
            // so the first shades and the second displaces
            auto diffuseImage = LoadImage("Content/Assets/3crates/crate1/crate1_diffuse.ktx2");
            auto normalImage = LoadImage("Content/Assets/3crates/crate1/crate1_normal.ktx2");
            auto bumpImage = LoadImage("Content/Assets/3crates/crate1/crate1_bump.ktx2");

            diffuse = device.CreateTexture(RHITextureCreateDesc{
                .width = diffuseImage.width, .height = diffuseImage.height,
                .mipLevels = diffuseImage.mipLevels,
                .arraySize = diffuseImage.arraySize,
                .format = diffuseImage.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = diffuseImage.subs
            });
            normal = device.CreateTexture(RHITextureCreateDesc{
                .width = normalImage.width, .height = normalImage.height,
                .mipLevels = normalImage.mipLevels,
                .arraySize = normalImage.arraySize,
                .format = normalImage.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = normalImage.subs
            });
            bump = device.CreateTexture(RHITextureCreateDesc{
                .width = bumpImage.width, .height = bumpImage.height,
                .mipLevels = bumpImage.mipLevels,
                .arraySize = bumpImage.arraySize,
                .format = bumpImage.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = bumpImage.subs
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

            const auto dpos = input.GetMouseDPos();
            const auto angle = norm(dpos) * SENSITIVITY;
            if(angle == 0.0f)
                return;

            const auto axis = normalize(Vec3{-dpos.y, -dpos.x, 0.0f});
            boxRot = normalize(quat(axisAngle(axis, angle), boxRot));
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            constexpr auto view = viewMat(eye, unitQuat());
            const auto proj = perspective(fovY, aspect, nearZ, farZ);

            Uniforms uniforms{
                .model = rotateMat(boxRot),
                .viewProj = proj * view,
                .cameraPos = eye,
                .toLight = toLight
            };
            uniformsCB->Upload(uniforms);

            std::array colorAttachments = {
                backBuffer
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
                .diffuse = diffuse->GetReadableID(),
                .normal = normal->GetReadableID(),
                .bump = bump->GetReadableID()
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
        .title = "NormalMapping",
        .width = 800, .height = 800,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<NormalMapping>(windowConfig);
}
