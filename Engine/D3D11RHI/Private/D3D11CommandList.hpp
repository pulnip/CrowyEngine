#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <d3d11.h>
#include "RHITextureView.hpp"
#include "assert.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif
#include "D3D11Buffer.hpp"
#include "D3D11BufferView.hpp"
#include "D3D11PipelineState.hpp"
#include "D3D11Sampler.hpp"
#include "D3D11Swapchain.hpp"
#include "D3D11Texture.hpp"
#include "D3D11TextureView.hpp"

namespace Crowy
{
    class D3D11CommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
    private:
        // NOTE. Immediate context.
        ID3D11DeviceContext* context = nullptr;
        bool isRecording = false;
        bool isRenderPass = false, isComputePass = false;
    #if defined(_DEBUG) || !defined(NDEBUG)
        uint32_t maxBindedVSSRV = 0;
        uint32_t maxBindedPSSRV = 0;
        uint32_t maxBindedCSSRV = 0;
    #endif

    public:
        D3D11CommandList(
            ID3D11Device* device,
            ID3D11DeviceContext* context
        )
            : context(context)
        {}

        ~D3D11CommandList() = default;

        void begin() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(!isRecording,
                "Did you call RHICommandList::close()?"
            );
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
            std::span<RHITextureView*> renderTargetViews,
            RHITextureView* depthStencilView,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderTargetViews.size() > 0);

            ID3D11RenderTargetView* rtvs[RHI_MAX_RENDER_TARGETS];
            for(size_t i=0; i<renderTargetViews.size(); ++i)
                rtvs[i] = static_cast<D3D11TextureRTV*>(renderTargetViews[i])->get();

            ID3D11DepthStencilView* dsv = depthStencilView != nullptr ?
                static_cast<D3D11TextureDSV*>(depthStencilView)->get() : nullptr;

            beginRenderPass(
                std::span<ID3D11RenderTargetView*>(rtvs, renderTargetViews.size()),
                dsv,
                loadAction, storeAction,
                clearColor, clearDS,
                debugName
            );
        }

        void beginRenderPass(
            RHISwapchain& swapchain,
            RHITextureView* depthStencilView,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{
            ID3D11RenderTargetView* rtvs[1] = {
                static_cast<D3D11Swapchain&>(swapchain).getCurrentRTV()
            };

            ID3D11DepthStencilView* dsv = depthStencilView != nullptr ?
                static_cast<D3D11TextureDSV*>(depthStencilView)->get() : nullptr;

            beginRenderPass(
                rtvs,
                dsv,
                loadAction, storeAction,
                clearColor, clearDS,
                debugName
            );
        }

        void endRenderPass() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRenderPass,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            // NOTE. No-Op for D3D11
        #if defined(_DEBUG) || !defined(NDEBUG)
            // clean-up srv binding for suppress data hazard warning.
            static ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};

            if(maxBindedVSSRV > 0)
                context->VSSetShaderResources(0, maxBindedVSSRV, nullSRVs);
            if(maxBindedPSSRV > 0)
                context->PSSetShaderResources(0, maxBindedPSSRV, nullSRVs);
            if(maxBindedCSSRV > 0)
                context->CSSetShaderResources(0, maxBindedCSSRV, nullSRVs);

            maxBindedVSSRV = 0;
            maxBindedPSSRV = 0;
            maxBindedCSSRV = 0;
        #endif

            isRenderPass = false;
        }

        void setPipelineState(RHIGraphicsPipelineState& pso) noexcept RHI_OVERRIDE{
            auto& dxPSO = static_cast<D3D11GraphicsPipelineState&>(pso);

            auto il = dxPSO.getIL();
            if(il != nullptr)
                context->IASetInputLayout(il);
            context->IASetPrimitiveTopology(dxPSO.getTopology());
            context->VSSetShader(dxPSO.getVS(), nullptr, 0);
            context->PSSetShader(dxPSO.getPS(), nullptr, 0);
            context->RSSetState(dxPSO.getRS());
            context->OMSetDepthStencilState(dxPSO.getDSS(), 0);
            context->OMSetBlendState(dxPSO.getBS(), nullptr, 0xFFFFFFFF);
        }

        void setPipelineState(RHIComputePipelineState& pso) noexcept RHI_OVERRIDE{
            auto& dxPSO = static_cast<D3D11ComputePipelineState&>(pso);
            
            // TODO.
        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto buf = static_cast<D3D11Buffer&>(buffer).get();
            context->IASetVertexBuffers(
                slot,
                1,
                &buf,
                &stride,
                &offset
            );
        }

        void setIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto buf = static_cast<D3D11Buffer&>(buffer).get();
            context->IASetIndexBuffer(
                buf,
                format == RHIIndexFormat::UInt16 ?
                    DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
                offset
            );
        }

        void setConstantBuffer(
            RHIShaderStage stage,
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto buf = static_cast<D3D11Buffer&>(buffer).get();

            switch(stage){
            case RHIShaderStage::VertexShader:
                context->VSSetConstantBuffers(slot, 1, &buf);
                break;
            case RHIShaderStage::FragmentShader:
                context->PSSetConstantBuffers(slot, 1, &buf);
                break;
            case RHIShaderStage::ComputeShader:
                context->CSSetConstantBuffers(slot, 1, &buf);
                break;
            default:
                std::unreachable();
            }
        }

        void setTexture(
            uint32_t slot,
            RHITextureView& textureView,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            using enum RHIShaderStage;
            const auto view = static_cast<D3D11TextureSRV&>(textureView).get();

            switch(stage){
            case VertexShader:
            #if defined(_DEBUG) || !defined(NDEBUG)
                maxBindedVSSRV = std::max(maxBindedVSSRV, slot+1);
            #endif
                context->VSSetShaderResources(slot, 1, &view);
                break;
            case FragmentShader:
            #if defined(_DEBUG) || !defined(NDEBUG)
                maxBindedPSSRV = std::max(maxBindedPSSRV, slot+1);
            #endif
                context->PSSetShaderResources(slot, 1, &view);
                break;
            case ComputeShader:
            #if defined(_DEBUG) || !defined(NDEBUG)
                maxBindedCSSRV = std::max(maxBindedCSSRV, slot+1);
            #endif
                context->CSSetShaderResources(slot, 1, &view);
                break;
            default:
                std::unreachable();
            }
        }

        void setBuffer(
            uint32_t slot,
            RHIBufferView& bufferView,
            RHIShaderStage stage = RHIShaderStage::ComputeShader
        ) noexcept RHI_OVERRIDE{
            using enum RHIBindingAccess;
            using enum RHIShaderStage;

            if(bufferView.getAccess() == ReadOnly){
                ID3D11ShaderResourceView* const views = {
                    static_cast<D3D11BufferSRV&>(bufferView).get()
                };

                switch(stage){
                case VertexShader:
                    context->VSSetShaderResources(
                        slot,
                        1,
                        &views
                    );
                    break;
                case FragmentShader:
                    context->PSSetShaderResources(
                        slot,
                        1,
                        &views
                    );
                    break;
                case ComputeShader:
                    context->CSSetShaderResources(
                        slot,
                        1,
                        &views
                    );
                    break;
                default:
                    std::unreachable();
                }
            }
            else{
                // cannot bind to VS, GS, HS, DS, TS
                // TODO. bind to PS is available at OMSetRenderTargetsAndUnorderedAccessViews
                CROWY_ASSERT(stage == ComputeShader);
                // WriteOnly / ReadWrite
                ID3D11UnorderedAccessView* const views = {
                    static_cast<D3D11BufferUAV&>(bufferView).get()
                };

                context->CSSetUnorderedAccessViews(
                    slot,
                    1,
                    &views,
                    nullptr
                );
            }
        }

        void setBytes(
            uint32_t slot,
            const void* bytes,
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
            uint32_t slot,
            RHISampler& sampler,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            auto s = static_cast<D3D11Sampler&>(sampler).get();

            switch(stage){
            case RHIShaderStage::VertexShader:
                context->VSSetSamplers(slot, 1, &s);
                break;
            case RHIShaderStage::FragmentShader:
                context->PSSetSamplers(slot, 1, &s);
                break;
            case RHIShaderStage::ComputeShader:
                context->CSSetSamplers(slot, 1, &s);
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
            context->RSSetViewports(1, &vp);
        }

        void setScissorRect(const RHIScissorRect& scissor) noexcept RHI_OVERRIDE{
            D3D11_RECT rect{
                .left = scissor.left,
                .top = scissor.top,
                .right = scissor.right,
                .bottom = scissor.bottom
            };
            context->RSSetScissorRects(1, &rect);
        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            if(instanceCount > 1)
                context->DrawInstanced(
                    vertexCount,
                    instanceCount,
                    startVertex,
                    startInstance
                );
            else
                context->Draw(vertexCount, startVertex);
        }

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            if(instanceCount > 1)
                context->DrawIndexedInstanced(
                    indexCount,
                    instanceCount,
                    startIndex,
                    baseVertex,
                    startInstance
                );
            else
                context->DrawIndexed(
                    indexCount,
                    startIndex,
                    baseVertex
                );
        }

        void beginCompute() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(!isComputePass,
                "Did you call RHICommandList::beginCompute()?"
            );
            // NOTE. No-Op for D3D11

            isComputePass = true;
        }

        void endCompute() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isComputePass,
                "Did you call RHICommandList::beginCompute()?"
            );
            // NOTE. No-Op for D3D11

            isComputePass = false;
        }

        void dispatch(
            RHISize3D gridSize
        ) noexcept RHI_OVERRIDE{
            // TODO. reflection from shader?
            auto threadGroupSize = RHISize3D{256, 1, 1};

            context->Dispatch(
                gridSize.x / threadGroupSize.x,
                gridSize.y / threadGroupSize.y,
                gridSize.z / threadGroupSize.z
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

            context->CopySubresourceRegion(
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
            context->CopyResource(
                static_cast<D3D11Texture&>(dst).get(),
                static_cast<D3D11Texture&>(src).get()
            );
        }

        void copy(
            RHITexture& src,
            RHISwapchain& dst
        ) noexcept RHI_OVERRIDE{
            context->CopyResource(
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
            context->OMSetRenderTargets(rtvs.size(), rtvs.data(), dsv);

            if(loadAction == RHILoadAction::Clear){
                float cc[4] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};

                for(size_t i=0; i<rtvs.size(); ++i)
                    context->ClearRenderTargetView(rtvs[i], cc);

                if(dsv != nullptr)
                    context->ClearDepthStencilView(dsv,
                        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                        clearDS.depth, clearDS.stencil
                    );
            }

            isRenderPass = true;
        }
    };
}
