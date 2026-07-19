#include <array>
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
        Crowy::Vec3 tangent;
        Crowy::Vec2 texCoord;
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
        },
        Crowy::RHIVertexElement{
            .semanticName = "TANGENT",
            .semanticIndex = 0,
            .format = Crowy::RHIPixelFormat::RGB32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 24,
            .classification = Crowy::RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        Crowy::RHIVertexElement{
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = Crowy::RHIPixelFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 36,
            .classification = Crowy::RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        }
    };

    struct MeshData{
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    MeshData makeBox(float halfSize = 1.0f){
        using namespace Crowy;

        struct Face{
            Crowy::Vec3 normal;
            // direction of increasing u.
            Crowy::Vec3 tangent;
        };
        constexpr std::array faces = {
            Face{.normal = { 1.0f,  0.0f,  0.0f}, .tangent = { 0.0f, 0.0f,  1.0f}},
            Face{.normal = {-1.0f,  0.0f,  0.0f}, .tangent = { 0.0f, 0.0f, -1.0f}},
            Face{.normal = { 0.0f,  1.0f,  0.0f}, .tangent = { 1.0f, 0.0f,  0.0f}},
            Face{.normal = { 0.0f, -1.0f,  0.0f}, .tangent = { 1.0f, 0.0f,  0.0f}},
            Face{.normal = { 0.0f,  0.0f,  1.0f}, .tangent = {-1.0f, 0.0f,  0.0f}},
            Face{.normal = { 0.0f,  0.0f, -1.0f}, .tangent = { 1.0f, 0.0f,  0.0f}}
        };
        constexpr std::array<Crowy::Vec2, 4> texCoords = {
            Crowy::Vec2{0.0f, 0.0f},
            Crowy::Vec2{1.0f, 0.0f},
            Crowy::Vec2{1.0f, 1.0f},
            Crowy::Vec2{0.0f, 1.0f}
        };

        std::vector<Vertex> vertices;
        vertices.reserve(faces.size() * texCoords.size());

        std::vector<u32> indices;
        indices.reserve(faces.size() * 6);

        for(const auto& [normal, tangent]: faces){
            const auto bitangent = cross(normal, tangent);
            const auto base = static_cast<u32>(vertices.size());

            for(const auto& uv: texCoords){
                const float su = 2.0f * (uv.x - 0.5f) * halfSize;
                const float sv = 2.0f * (uv.y - 0.5f) * halfSize;

                vertices.push_back(Vertex{
                    .position = halfSize*normal + su*tangent + sv*bitangent,
                    .normal = normal,
                    .tangent = tangent,
                    .texCoord = uv
                });
            }

            indices.insert(indices.end(), {
                base + 0, base + 1, base + 2,
                base + 0, base + 2, base + 3
            });
        }

        return MeshData{
            .vertices = std::move(vertices),
            .indices = std::move(indices)
        };
    }
}

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
                    .vertexLayout = ::VERTEX_INPUT_LAYOUT,
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

            auto boxMesh = ::makeBox(1.0f);
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
