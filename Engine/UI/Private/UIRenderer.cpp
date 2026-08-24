#include <algorithm>
#include <array>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>
#include <imgui.h>
#include "Assert.hpp"
#include "IntMath.hpp"
#include "PtrUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDebugScope.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"
#include "UIRenderer.hpp"

namespace{
    using namespace Crowy;

    inline constexpr std::array UI_VERTEX_LAYOUT = {
        RHIVertexElement{
            .semanticName = "POSITION",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(ImDrawVert, pos)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(ImDrawVert, uv)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "COLOR",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGBA8_UNORM,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(ImDrawVert, col)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        }
    };

    inline constexpr auto UI_INDEX_FORMAT = sizeof(ImDrawIdx) == 2 ?
        RHIIndexFormat::UInt16 :
        RHIIndexFormat::UInt32;


    // a render target that encodes on write needs colors handed to it linear
    constexpr bool encodesOnWrite(RHIPixelFormat format) noexcept{
        using enum RHIPixelFormat;

        switch(format){
        case RGBA8_UNORM_SRGB: [[fallthrough]];
        case BGRA8_UNORM_SRGB:
            return true;
        default:
            return false;
        }
    }

    struct PushConstants{
        u64 texture = 0;
        Vec2 scale;
        Vec2 translate;
        u32 srgbTarget = 0;
        u32 padding = 0;
    };

    bool clip(
        RHICommandList& cmdList,
        const ImDrawData& drawData,
        const ImDrawCmd& cmd,
        Vec2 framebuffer
    ){
        const auto& origin = drawData.DisplayPos;
        const auto& scale = drawData.FramebufferScale;

        const Vec2 min{
            (cmd.ClipRect.x - origin.x) * scale.x,
            (cmd.ClipRect.y - origin.y) * scale.y
        };
        const Vec2 max{
            (cmd.ClipRect.z - origin.x) * scale.x,
            (cmd.ClipRect.w - origin.y) * scale.y
        };
        if(max.x <= min.x || max.y <= min.y)
            return false;

        cmdList.SetScissorRect(RHIScissorRect{
            .left = static_cast<i32>(std::max(min.x, 0.0f)),
            .top = static_cast<i32>(std::max(min.y, 0.0f)),
            .right = static_cast<i32>(std::min(max.x, framebuffer.x)),
            .bottom = static_cast<i32>(std::min(max.y, framebuffer.y))
        });

        return true;
    }
}

namespace Crowy
{
    UIRenderer::UIRenderer(
        RHIDevice& device,
        RHIPixelFormat renderTargetFormat,
        RHIPixelFormat depthFormat
    )
        : device(device)
        , capabilities(device.GetCapabilities())
        , srgbTarget(encodesOnWrite(renderTargetFormat) ? 1u : 0u)
    {
        IMGUI_CHECKVERSION();
        context = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        CROWY_ASSERT(context != nullptr,
            "Did you create an ImGui context first?"
        );

        auto& io = ImGui::GetIO();
        CROWY_ASSERT(io.BackendRendererUserData == nullptr,
            "a renderer backend is already installed"
        );
        io.BackendRendererName = "CrowyUIRenderer";
        io.BackendRendererUserData = this;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.Fonts->AddFontFromFileTTF(
            "Content/Fonts/NanumBarunGothic.ttf",
            18.0f
        );

        // dockspace with BeginDockSpace()
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
            .preRasterizer = RHILegacyFrontendDesc{
                .vertexLayout = UI_VERTEX_LAYOUT,
                .topology = RHIPrimitiveTopology::TriangleList,
                .vertexShader = {
                    .path = "Engine/Shader/UI.slang",
                    .entryPoint = "vs_main"
                }
            },
            .rasterizer = RHIRasterizerState{
                .cullMode = RHICullMode::None
            },
            .fragmentShader = {
                .path = "Engine/Shader/UI.slang",
                .entryPoint = "fs_main"
            },
            // UI ignores the scene's depth but the formats must still agree
            .depthStencil = depthFormat == RHIPixelFormat::Unknown ?
                std::nullopt :
                std::optional{RHIDepthStencilState{
                    .format = depthFormat,
                    .depthWriteEnable = false,
                    .depthFunc = RHIComparisonFunc::Always
                }},
            .blend = RHIBlendState{
                .renderTargets = {
                    RHIRenderTargetBlendState{
                        .blendEnable = true,
                        .srcBlend = RHIBlend::SrcAlpha,
                        .dstBlend = RHIBlend::InvSrcAlpha,
                        .blendOp = RHIBlendOp::Add,
                        .srcBlendAlpha = RHIBlend::One,
                        .dstBlendAlpha = RHIBlend::InvSrcAlpha,
                        .blendOpAlpha = RHIBlendOp::Add
                    }
                }
            },
            .renderTargetFormats = {
                renderTargetFormat
            },
            .renderTargetCount = 1
        }, "UIRenderer");
    }

    UIRenderer::~UIRenderer(){
        for(auto tex: ImGui::GetPlatformIO().Textures){
            if(tex->RefCount == 1)
                destroyTexture(*tex);
        }

        auto& io = ImGui::GetIO();
        io.BackendRendererName = nullptr;
        io.BackendRendererUserData = nullptr;
        io.BackendFlags &= ~ImGuiBackendFlags_RendererHasTextures;
        io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;

        ImGui::DestroyContext(context);
    }

    u32 UIRenderer::BeginDockSpace(){
        return ImGui::DockSpaceOverViewport(
            0, nullptr,
            ImGuiDockNodeFlags_PassthruCentralNode
        );
    }

    void UIRenderer::Prepare(
        RHICommandList& cmdList,
        Widget& widget,
        UIContext& context
    ){
        ImGui::Begin("Crowy");
        std::visit([&ctx = context](auto& w){
            w.submit(ctx);
        }, widget);
        ImGui::End();

        ImGui::Render();

        RHIEventScope event(cmdList, "UI Upload");

        textureAcquires.clear();
        updateTextures(cmdList);
        uploadGeometry();
    }

    void UIRenderer::Record(
        RHICommandList& cmdList
    ){
        RHIEventScope event(cmdList, "UI");

        auto& drawData = *ImGui::GetDrawData();

        const Vec2 framebuffer{
            drawData.DisplaySize.x * drawData.FramebufferScale.x,
            drawData.DisplaySize.y * drawData.FramebufferScale.y
        };

        if(framebuffer.x <= 0.0f || framebuffer.y <= 0.0f)
            return;
        if(drawData.TotalVtxCount == 0)
            return;

        CROWY_ASSERT(vertices.IsValid() && indices.IsValid(),
            "Did you call UIRenderer::Prepare() with this frame's draw data?"
        );

        PushConstants pushConstants{
            .scale = {
                2.0f / drawData.DisplaySize.x,
                -2.0f / drawData.DisplaySize.y
            },
            .srgbTarget = srgbTarget
        };
        pushConstants.translate = {
            -1.0f - drawData.DisplayPos.x * pushConstants.scale.x,
             1.0f - drawData.DisplayPos.y * pushConstants.scale.y
        };

        setupRenderState(cmdList, framebuffer);

        // every command in every list indexes the same slice;
        // only the element range moves
        const RHIIndexBufferView indexView{
            .buffer = indices.buffer,
            .format = UI_INDEX_FORMAT,
            .offset = indices.offset
        };

        // the lists share one pair of buffers, so their offsets accumulate
        u32 globalVertexOffset = 0, globalIndexOffset = 0;
        for(const ImDrawList* list: drawData.CmdLists){
            for(const auto& cmd: list->CmdBuffer){
                if(cmd.UserCallback != nullptr){
                    if(cmd.UserCallback == ImDrawCallback_ResetRenderState)
                        setupRenderState(cmdList, framebuffer);
                    else
                        cmd.UserCallback(list, &cmd);

                    continue;
                }

                if(!::clip(cmdList, drawData, cmd, framebuffer))
                    continue;

                pushConstants.texture = cmd.GetTexID();
                cmdList.SetPushGraphicsConstants(pushConstants);

                cmdList.DrawIndexed(
                    indexView,
                    cmd.ElemCount,
                    1,
                    cmd.IdxOffset + globalIndexOffset,
                    static_cast<i32>(cmd.VtxOffset + globalVertexOffset)
                );
            }

            globalVertexOffset += static_cast<u32>(list->VtxBuffer.Size);
            globalIndexOffset += static_cast<u32>(list->IdxBuffer.Size);
        }
    }

    void UIRenderer::setupRenderState(
        RHICommandList& cmdList,
        Vec2 framebuffer
    ){
        cmdList.SetViewport(RHIViewport{
            .x = 0.0f, .y = 0.0f,
            .width = framebuffer.x, .height = framebuffer.y,
            .minDepth = 0.0f, .maxDepth = 1.0f
        });

        cmdList.SetPipelineState(*pso);
        cmdList.SetVertexBuffer(
            *vertices.buffer,
            0,
            static_cast<u32>(sizeof(ImDrawVert)),
            vertices.offset
        );
    }

    void UIRenderer::updateTextures(
        RHICommandList& cmdList
    ){
        auto& drawData = *ImGui::GetDrawData();

        if(drawData.Textures == nullptr)
            return;

        for(auto* tex: *drawData.Textures){
            switch(tex->Status){
            case ImTextureStatus_WantCreate:
                createTexture(*tex);
                break;
            case ImTextureStatus_WantUpdates:
                updateTexture(cmdList, *tex);
                break;
            case ImTextureStatus_WantDestroy:
                if(tex->UnusedFrames >= RHI_FRAMES_IN_FLIGHT)
                    destroyTexture(*tex);
                break;
            default:
                break;
            }
        }
    }

    void UIRenderer::createTexture(
        ImTextureData& tex
    ){
        CROWY_ASSERT(tex.Format == ImTextureFormat_RGBA32,
            "the UI renderer only uploads RGBA32 textures"
        );
        CROWY_ASSERT(tex.TexID == ImTextureID_Invalid,
            "texture was created twice"
        );

        const std::array initialData = {
            RHISubresourceData{
                .data = tex.GetPixels(),
                .rowPitch = static_cast<usize>(tex.GetPitch())
            }
        };
        auto texture = device.CreateTexture(RHITextureCreateDesc{
            .width = static_cast<u32>(tex.Width),
            .height = static_cast<u32>(tex.Height),
            .format = RHIPixelFormat::RGBA8_UNORM,
            .usage = RHITextureUsage::ShaderResource,
            .initialData = initialData
        }, "ImGui texture");

        const auto id = texture->GetReadableID();

        const auto [it, inserted] = textures.emplace(id, std::move(texture));
        CROWY_ASSERT(inserted, "a live texture already claims this handle");

        tex.SetTexID(id);
        tex.SetStatus(ImTextureStatus_OK);
    }

    void UIRenderer::updateTexture(RHICommandList& cmdList, ImTextureData& tex){
        CROWY_ASSERT(tex.Format == ImTextureFormat_RGBA32,
            "the UI renderer only uploads RGBA32 textures"
        );

        auto it = textures.find(tex.TexID);
        CROWY_ASSERT(it != textures.end(),
            "ImGui asked to update a texture we never created"
        );
        auto& texture = *it->second;

        const auto& rect = tex.UpdateRect;
        const u32 left = rect.x, top = rect.y;
        const u32 width = rect.w, height = rect.h;

        const auto bytesPerPixel = static_cast<u32>(tex.BytesPerPixel);
        const u32 rowSize = width * bytesPerPixel;
        const u32 rowPitch = nextMul(
            rowSize,
            capabilities.textureRowPitchAlign
        );

        auto staging = device.CreateBuffer(RHIBufferCreateDesc{
            .size = rowPitch * height,
            .memory = RHIMemoryType::CPUWrite
        }, "ImGui texture update");

        for(u32 row=0; row<height; ++row){
            staging->Upload(
                tex.GetPixelsAt(
                    static_cast<int>(left),
                    static_cast<int>(top + row)
                ),
                rowSize,
                row * rowPitch
            );
        }

        // the previous readers live in earlier submissions' UI passes;
        // a partial rect update must keep the untouched texels, so no discard
        const std::array acquires{
            MakeCrossSubmissionBarrier(
                texture,
                RHIResourceUsage::SampledFragment,
                RHIResourceUsage::CopyDst
            )
        };
        cmdList.BeginBlitPass(acquires);
        cmdList.Copy(
            *staging,
            0,
            rowPitch,
            texture,
            RHITextureRegion{
                .x = left, .y = top,
                .width = width, .height = height
            }
        );
        // this frame's UI pass samples the texture again - a real edge whose
        // acquire half goes back to the caller through TextureAcquires()
        const std::array releases{
            MakeBarrier(
                texture,
                RHIResourceUsage::CopyDst,
                RHIResourceUsage::SampledFragment
            )
        };
        cmdList.EndBlitPass(releases);
        textureAcquires.push_back(releases[0]);

        device.Retire(std::move(staging));
        tex.SetStatus(ImTextureStatus_OK);
    }

    void UIRenderer::destroyTexture(ImTextureData& tex){
        if(auto it = textures.find(tex.TexID); it != textures.end()){
            device.Retire(std::move(it->second));
            textures.erase(it);
        }

        tex.SetTexID(ImTextureID_Invalid);
        tex.SetStatus(ImTextureStatus_Destroyed);
    }

    void UIRenderer::uploadGeometry(){
        auto& drawData = *ImGui::GetDrawData();

        if(drawData.TotalVtxCount == 0)
            return;

        // ImGui rebuilds its whole geometry every frame, so it never wants a
        // buffer of its own - one slice per frame, sized to what came out
        vertices = device.AllocateTransient(
            static_cast<u32>(drawData.TotalVtxCount * sizeof(ImDrawVert)),
            static_cast<u32>(sizeof(ImDrawVert))
        );
        indices = device.AllocateTransient(
            static_cast<u32>(drawData.TotalIdxCount * sizeof(ImDrawIdx)),
            static_cast<u32>(sizeof(ImDrawIdx))
        );

        u32 vertexOffset = 0, indexOffset = 0;
        for(const ImDrawList* list: drawData.CmdLists){
            const auto vertexBytes = static_cast<u32>(
                list->VtxBuffer.Size * sizeof(ImDrawVert)
            );
            const auto indexBytes = static_cast<u32>(
                list->IdxBuffer.Size * sizeof(ImDrawIdx)
            );

            if(vertexBytes > 0){
                std::memcpy(
                    ptrAdd(vertices.cpuPtr, vertexOffset),
                    list->VtxBuffer.Data,
                    vertexBytes
                );
            }
            if(indexBytes > 0){
                std::memcpy(
                    ptrAdd(indices.cpuPtr, indexOffset),
                    list->IdxBuffer.Data,
                    indexBytes
                );
            }

            vertexOffset += vertexBytes;
            indexOffset += indexBytes;
        }
    }
}
