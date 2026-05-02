#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <d3d11.h>
#include "assert.hpp"
#include "int_math.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif
#include "D3D11Buffer.hpp"
#include "D3D11Definitions.hpp"
#include "D3D11PipelineState.hpp"
#include "D3D11Sampler.hpp"
#include "D3D11Swapchain.hpp"
#include "D3D11Texture.hpp"

namespace Crowy
{
    class D3D11CommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
    private:
        // NOTE. Immediate context.
        ID3D11DeviceContext& context;
        // simulate command recording
        bool isRecording = false;
        bool inRenderPass = false, inComputePass = false;
        D3D11ComputePipelineState* currentComputePSO = nullptr;
    #if defined(_DEBUG) || !defined(NDEBUG)
        uint32_t maxBindedVSSRV = 0;
        uint32_t maxBindedPSSRV = 0;
        uint32_t maxBindedCSSRV = 0;
    #endif

    public:
        D3D11CommandList(ID3D11DeviceContext& context)
            : context(context){}

        ~D3D11CommandList() = default;

        void begin() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(!isRecording,
                "Did you call RHICommandList::close()?"
            );
            CROWY_ASSERT(!inRenderPass && !inComputePass);

            // NOTE. No-Op for D3D11
            isRecording = true;
        }

        void flush() noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void close() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(!inRenderPass && !inComputePass);

            // NOTE. No-Op for D3D11
            isRecording = false;
        }

        void reset() noexcept RHI_OVERRIDE{
            if(isRecording){
                flush();
                isRecording = false;
            }
        }

        void beginRenderPass(
            std::span<RHITexture*> renderTargets,
            RHITexture* depthTarget,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(!inRenderPass,
                "Already in a render pass. Did you call RHICommandList::endRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass,
                "Already in a compute pass. Did you call RHICommandList::endComputePass()?"
            );
            CROWY_ASSERT(renderTargets.size() > 0);

            RTV* rtvs[RHI_MAX_RENDER_TARGETS];
            for(size_t i=0; i<renderTargets.size(); ++i){
                auto tex = static_cast<D3D11Texture*>(renderTargets[i]);
                rtvs[i] = tex->getOrCreateRTV({.format = tex->getFormat()});
            }

            DSV* dsv = nullptr;
            if(depthTarget != nullptr){
                auto tex = static_cast<D3D11Texture*>(depthTarget);
                dsv = tex->getOrCreateDSV({.format = tex->getFormat()});
            }

            beginRenderPass(
                std::span<RTV*>(rtvs, renderTargets.size()),
                dsv,
                loadAction, storeAction,
                clearColor, clearDS,
                debugName
            );

            inRenderPass = true;
        }

        void beginRenderPass(
            RHISwapchain& swapchain,
            RHITexture* depthTarget,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(!inRenderPass,
                "Already in a render pass. Did you call RHICommandList::endRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass,
                "Already in a compute pass. Did you call RHICommandList::endComputePass()?"
            );
            ID3D11RenderTargetView* rtvs[1] = {
                static_cast<D3D11Swapchain&>(swapchain).getCurrentRTV()
            };

            DSV* dsv = nullptr;
            if(depthTarget != nullptr){
                auto tex = static_cast<D3D11Texture*>(depthTarget);
                dsv = tex->getOrCreateDSV({.format = tex->getFormat()});
            }

            beginRenderPass(
                rtvs,
                dsv,
                loadAction, storeAction,
                clearColor, clearDS,
                debugName
            );

            inRenderPass = true;
        }

        void endRenderPass() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass,
                "Not in a render pass. Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass);

            // NOTE. No-Op for D3D11
        #if defined(_DEBUG) || !defined(NDEBUG)
            // clean-up srv binding for suppress data hazard warning.
            static ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};

            if(maxBindedVSSRV > 0)
                context.VSSetShaderResources(0, maxBindedVSSRV, nullSRVs);
            if(maxBindedPSSRV > 0)
                context.PSSetShaderResources(0, maxBindedPSSRV, nullSRVs);
            if(maxBindedCSSRV > 0)
                context.CSSetShaderResources(0, maxBindedCSSRV, nullSRVs);

            maxBindedVSSRV = 0;
            maxBindedPSSRV = 0;
            maxBindedCSSRV = 0;

            // Unbind render targets to avoid warnings about resources being bound while being used.
            context.OMSetRenderTargets(0, nullptr, nullptr);
        #endif

            inRenderPass = false;
        }

        void setPipelineState(RHIGraphicsPipelineState& pso) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass,
                "Not in a render pass. Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass);

            auto& dxPSO = static_cast<D3D11GraphicsPipelineState&>(pso);
            dxPSO.bind(context);
        }

        void setPipelineState(RHIComputePipelineState& pso) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inComputePass,
                "Not in a compute pass. Did you call RHICommandList::beginComputePass()?"
            );
            CROWY_ASSERT(!inRenderPass);

            auto& dxPSO = static_cast<D3D11ComputePipelineState&>(pso);
            dxPSO.bind(context);

            currentComputePSO = &dxPSO;
        }

        void setVertexBuffer(
            const RHIBuffer& buffer,
            uint32_t slot,
            uint32_t stride,
            uint32_t offset
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass,
                "Not in a render pass. Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass);

            auto buf = static_cast<const D3D11Buffer&>(buffer).get();
            context.IASetVertexBuffers(
                slot,
                1,
                &buf,
                &stride,
                &offset
            );
        }

        void setIndexBuffer(
            const RHIBuffer& buffer,
            RHIIndexFormat format,
            uint32_t offset
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass,
                "Not in a render pass. Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass);

            auto buf = static_cast<const D3D11Buffer&>(buffer).get();
            context.IASetIndexBuffer(
                buf,
                format == RHIIndexFormat::UInt16 ?
                    DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
                offset
            );
        }

        void setConstantBuffer(
            const RHIBuffer& buffer,
            uint32_t slot,
            RHIShaderStage stage,
            uint32_t offset
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass != inComputePass,
                "Not in a pass. Did you call RHICommandList::beginPass()?"
            );

            auto buf = static_cast<const D3D11Buffer&>(buffer).get();

            switch(stage){
            case RHIShaderStage::VertexShader:
                context.VSSetConstantBuffers(slot, 1, &buf);
                break;
            case RHIShaderStage::FragmentShader:
                context.PSSetConstantBuffers(slot, 1, &buf);
                break;
            case RHIShaderStage::ComputeShader:
                context.CSSetConstantBuffers(slot, 1, &buf);
                break;
            default:
                std::unreachable();
            }
        }

        void setTexture(
            RHITexture& texture,
            uint32_t slot,
            RHIBindingAccess access,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass != inComputePass,
                "Not in a pass. Did you call RHICommandList::beginPass()?"
            );

            using enum RHIBindingAccess;
            using enum RHIShaderStage;
            auto& dxTex = static_cast<D3D11Texture&>(texture);

            switch(access){
            case ReadOnly: {
                const auto view = dxTex.getOrCreateSRV({
                    .format = texture.getFormat()
                });
                switch(stage){
                case VertexShader:
                #if defined(_DEBUG) || !defined(NDEBUG)
                    maxBindedVSSRV = std::max(maxBindedVSSRV, slot+1);
                #endif
                    context.VSSetShaderResources(slot, 1, &view);
                    break;
                case FragmentShader:
                #if defined(_DEBUG) || !defined(NDEBUG)
                    maxBindedPSSRV = std::max(maxBindedPSSRV, slot+1);
                #endif
                    context.PSSetShaderResources(slot, 1, &view);
                    break;
                case ComputeShader:
                #if defined(_DEBUG) || !defined(NDEBUG)
                    maxBindedCSSRV = std::max(maxBindedCSSRV, slot+1);
                #endif
                    context.CSSetShaderResources(slot, 1, &view);
                    break;
                default:
                    std::unreachable();
                }
            } break;
            case ReadWrite: {
                CROWY_ASSERT(stage == ComputeShader);
                const auto view = dxTex.getOrCreateUAV({
                    .format = texture.getFormat()
                });
                switch(stage){
                case ComputeShader:
                    context.CSSetUnorderedAccessViews(slot, 1, &view, nullptr);
                    break;
                default:
                    std::unreachable();
                }
            } break;
            default:
                std::unreachable();
            }
        }

        void setBuffer(
            RHIBuffer& buffer,
            uint32_t slot,
            RHIBindingAccess access,
            RHIShaderStage stage = RHIShaderStage::ComputeShader
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass != inComputePass,
                "Not in a pass. Did you call RHICommandList::beginPass()?"
            );

            using enum RHIBindingAccess;
            using enum RHIShaderStage;
            auto& dxBuf = static_cast<D3D11Buffer&>(buffer);

            switch(access){
            case ReadOnly: {
                const auto view = dxBuf.getOrCreateSRV(RHIBufferViewDesc{
                    .size = buffer.getSize()
                });
                switch(stage){
                case VertexShader:
                #if defined(_DEBUG) || !defined(NDEBUG)
                    maxBindedVSSRV = std::max(maxBindedVSSRV, slot+1);
                #endif
                    context.VSSetShaderResources(slot, 1, &view);
                    break;
                case FragmentShader:
                #if defined(_DEBUG) || !defined(NDEBUG)
                    maxBindedPSSRV = std::max(maxBindedPSSRV, slot+1);
                #endif
                    context.PSSetShaderResources(slot, 1, &view);
                    break;
                case ComputeShader:
                #if defined(_DEBUG) || !defined(NDEBUG)
                    maxBindedCSSRV = std::max(maxBindedCSSRV, slot+1);
                #endif
                    context.CSSetShaderResources(slot, 1, &view);
                    break;
                default:
                    std::unreachable();
                }
            } break;
            case ReadWrite: {
                // cannot bind to VS, GS, HS, DS, TS
                // TODO. bind to PS is available at OMSetRenderTargetsAndUnorderedAccessViews
                CROWY_ASSERT(stage == ComputeShader);
                const auto view = dxBuf.getOrCreateUAV(RHIBufferViewDesc{
                    .size = buffer.getSize()
                });
                switch(stage){
                case ComputeShader:
                    context.CSSetUnorderedAccessViews(slot, 1, &view, nullptr);
                    break;
                default:
                    std::unreachable();
                }
            } break;
            default:
                std::unreachable();
            }
        }

        void setBytes(
            const void* bytes,
            uint32_t slot,
            size_t size,
            RHIShaderStage stage = RHIShaderStage::ComputeShader
        ) RHI_OVERRIDE{
            using enum RHIShaderStage;

            throw std::runtime_error("Unimplemented");

            switch(stage){
            case VertexShader:
                [[fallthrough]];
            case FragmentShader:
                break;
            case ComputeShader:
                break;
            default:
                std::unreachable();
            }
        }

        void setSampler(
            const RHISampler& sampler,
            uint32_t slot,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass != inComputePass,
                "Not in a pass. Did you call RHICommandList::beginPass()?"
            );

            using enum RHIShaderStage;
            auto s = static_cast<const D3D11Sampler&>(sampler).get();

            switch(stage){
            case VertexShader:
                context.VSSetSamplers(slot, 1, &s);
                break;
            case FragmentShader:
                context.PSSetSamplers(slot, 1, &s);
                break;
            case ComputeShader:
                context.CSSetSamplers(slot, 1, &s);
                break;
            default:
                std::unreachable();
            }   
        }

        void setViewport(const RHIViewport& viewport) noexcept RHI_OVERRIDE{
            D3D11_VIEWPORT vp{
                .TopLeftX = viewport.x,
                .TopLeftY = viewport.y,
                .Width = viewport.width,
                .Height = viewport.height,
                .MinDepth = viewport.minDepth,
                .MaxDepth = viewport.maxDepth
            };
            context.RSSetViewports(1, &vp);
        }

        void setScissorRect(const RHIScissorRect& scissor) noexcept RHI_OVERRIDE{
            D3D11_RECT rect{
                .left = scissor.left,
                .top = scissor.top,
                .right = scissor.right,
                .bottom = scissor.bottom
            };
            context.RSSetScissorRects(1, &rect);
        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass,
                "Not in a render pass. Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass);

            if(instanceCount > 1)
                context.DrawInstanced(
                    vertexCount,
                    instanceCount,
                    startVertex,
                    startInstance
                );
            else
                context.Draw(vertexCount, startVertex);
        }

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inRenderPass,
                "Not in a render pass. Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(!inComputePass);

            if(instanceCount > 1)
                context.DrawIndexedInstanced(
                    indexCount,
                    instanceCount,
                    startIndex,
                    baseVertex,
                    startInstance
                );
            else
                context.DrawIndexed(
                    indexCount,
                    startIndex,
                    baseVertex
                );
        }

        void beginCompute() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(!inComputePass,
                "Already in a compute pass. Did you call RHICommandList::endComputePass()?"
            );
            CROWY_ASSERT(!inRenderPass,
                "Already in a render pass. Did you call RHICommandList::endRenderPass()?"
            );

            inComputePass = true;
            currentComputePSO = nullptr;
        }

        void endCompute() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inComputePass,
                "Not in a compute pass. Did you call RHICommandList::beginComputePass()?"
            );
            CROWY_ASSERT(!inRenderPass);

            inComputePass = false;
        }

        void dispatch(
            RHISize3D gridSize
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );
            CROWY_ASSERT(inComputePass,
                "Not in a compute pass. Did you call RHICommandList::beginComputePass()?"
            );
            CROWY_ASSERT(currentComputePSO != nullptr,
                "Did you call RHICommandList::setPipelineState(ComputePSO)?"
            );
            auto threadGroupSize = currentComputePSO->getThreadGroupSize();

            context.Dispatch(
                ceil_div(gridSize.x, threadGroupSize.x),
                ceil_div(gridSize.y, threadGroupSize.y),
                ceil_div(gridSize.z, threadGroupSize.z)
            );
        }

        void transitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void transitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void uavBarrier(RHITexture&) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void uavBarrier(RHIBuffer&) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void signalFence(RHIFence&, uint64_t value) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void waitFence(RHIFence&, uint64_t value) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) noexcept RHI_OVERRIDE{
            D3D11_BOX box{
                .left = static_cast<UINT>(srcOffset),
                .top = 0,
                .front = 0,
                .right = static_cast<UINT>(srcOffset + size),
                .bottom = 1,
                .back = 1
            };

            context.CopySubresourceRegion(
                static_cast<D3D11Buffer&>(dst).get(), 0,
                dstOffset, 0, 0,
                static_cast<D3D11Buffer&>(src).get(), 0,
                &box
            );
        }

        void copy(
            RHITexture& src,
            RHITexture& dst
        ) noexcept RHI_OVERRIDE{
            context.CopyResource(
                static_cast<D3D11Texture&>(dst).get(),
                static_cast<D3D11Texture&>(src).get()
            );
        }

        void copy(
            RHITexture& src,
            RHISwapchain& dst
        ) noexcept RHI_OVERRIDE{
            context.CopyResource(
                static_cast<D3D11Swapchain&>(dst).getCurrentTexture(),
                static_cast<D3D11Texture&>(src).get()
            );
        }

        void copy(
            RHIBuffer& src,
            RHITexture& dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            // TODO
        }

        void waitUntilCompleted() noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        void beginEvent(const char* name) noexcept RHI_OVERRIDE{
            // TODO
        }

        void endEvent() noexcept RHI_OVERRIDE{
            // TODO
        }

        void setMarker(const char* name) noexcept RHI_OVERRIDE{
            // TODO
        }

    private:
        void beginRenderPass(
            std::span<ID3D11RenderTargetView*> rtvs,
            ID3D11DepthStencilView* dsv,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept{
            context.OMSetRenderTargets(rtvs.size(), rtvs.data(), dsv);

            if(loadAction == RHILoadAction::Clear){
                for(size_t i=0; i<rtvs.size(); ++i)
                    context.ClearRenderTargetView(rtvs[i], clearColor.v);

                if(dsv != nullptr)
                    context.ClearDepthStencilView(dsv,
                        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                        clearDS.depth, clearDS.stencil
                    );
            }
        }
    };
}
