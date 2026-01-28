#pragma once

#include <string>
#include <d3d11.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif
#include "D3D11Shader.hpp"
#include "D3D11Util.hpp"

namespace Crowy
{
    static D3D11_FILL_MODE convertFillMode(RHIFillMode mode){
        switch(mode){
        case RHIFillMode::Solid:     return D3D11_FILL_SOLID;
        case RHIFillMode::Wireframe: return D3D11_FILL_WIREFRAME;
        default:
            std::unreachable();
        }
    }

    static D3D11_CULL_MODE convertCullMode(RHICullMode mode){
        switch(mode){
        case RHICullMode::CullNone: return D3D11_CULL_NONE;
        case RHICullMode::Front:    return D3D11_CULL_FRONT;
        case RHICullMode::Back:     return D3D11_CULL_BACK;
        default:
            std::unreachable();
        }
    }

    D3D11_STENCIL_OP convertStencilOp(RHIStencilOp op){
        switch (op){
        case RHIStencilOp::Keep:     return D3D11_STENCIL_OP_KEEP;
        case RHIStencilOp::Zero:     return D3D11_STENCIL_OP_ZERO;
        case RHIStencilOp::Replace:  return D3D11_STENCIL_OP_REPLACE;
        case RHIStencilOp::IncrSat:  return D3D11_STENCIL_OP_INCR_SAT;
        case RHIStencilOp::DecrSat:  return D3D11_STENCIL_OP_DECR_SAT;
        case RHIStencilOp::Invert:   return D3D11_STENCIL_OP_INVERT;
        case RHIStencilOp::IncrWrap: return D3D11_STENCIL_OP_INCR;
        case RHIStencilOp::DecrWrap: return D3D11_STENCIL_OP_DECR;
        default:
            std::unreachable();
        }
    }

    auto convertStencilOpDesc(const RHIStencilOpDesc& desc) {
        return D3D11_DEPTH_STENCILOP_DESC{
            .StencilFailOp = convertStencilOp(desc.stencilFailOp),
            .StencilDepthFailOp = convertStencilOp(desc.depthFailOp),
            .StencilPassOp = convertStencilOp(desc.passOp),
            .StencilFunc = convertCompareFunc(desc.func),
        };
    }

    static D3D11_BLEND convertBlendFactor(RHIBlend blend){
        switch (blend) {
        case RHIBlend::Zero:          return D3D11_BLEND_ZERO;
        case RHIBlend::One:           return D3D11_BLEND_ONE;
        case RHIBlend::SrcColor:      return D3D11_BLEND_SRC_COLOR;
        case RHIBlend::InvSrcColor:   return D3D11_BLEND_INV_SRC_COLOR;
        case RHIBlend::SrcAlpha:      return D3D11_BLEND_SRC_ALPHA;
        case RHIBlend::InvSrcAlpha:   return D3D11_BLEND_INV_SRC_ALPHA;
        case RHIBlend::DestAlpha:     return D3D11_BLEND_DEST_ALPHA;
        case RHIBlend::InvDestAlpha:  return D3D11_BLEND_INV_DEST_ALPHA;
        case RHIBlend::DestColor:     return D3D11_BLEND_DEST_COLOR;
        case RHIBlend::InvDestColor:  return D3D11_BLEND_INV_DEST_COLOR;
        case RHIBlend::SrcAlphaSat:   return D3D11_BLEND_SRC_ALPHA_SAT;
        case RHIBlend::BlendFactor:   return D3D11_BLEND_BLEND_FACTOR;
        case RHIBlend::InvBlendFactor: return D3D11_BLEND_INV_BLEND_FACTOR;
        default:                      return D3D11_BLEND_ONE;
        }
    }

    static D3D11_BLEND_OP convertBlendOp(RHIBlendOp op){
        switch (op) {
        case RHIBlendOp::Add:             return D3D11_BLEND_OP_ADD;
        case RHIBlendOp::Subtract:        return D3D11_BLEND_OP_SUBTRACT;
        case RHIBlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case RHIBlendOp::Min:             return D3D11_BLEND_OP_MIN;
        case RHIBlendOp::Max:             return D3D11_BLEND_OP_MAX;
        default:                          return D3D11_BLEND_OP_ADD;
        }
    }

    static D3D11_PRIMITIVE_TOPOLOGY convertTopology(RHIPrimitiveTopology topology){
        switch(topology){
        case RHIPrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case RHIPrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case RHIPrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case RHIPrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case RHIPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            std::unreachable();
        }
    }

    class D3D11PipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    private:
        ID3D11InputLayout* il = nullptr;
        ID3D11RasterizerState* rs = nullptr;
        ID3D11DepthStencilState* dss = nullptr;
        ID3D11BlendState* bs = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY topology;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
    #if defined(_DEBUG) || !defined(NDEBUG)
        std::string debugName;
    #endif

    public:
        D3D11PipelineState(
            ID3D11Device* device,
            const RHIGraphicsPipelineStateDesc& desc
        )
            :topology(convertTopology(desc.topology))
        #if defined(_DEBUG) || !defined(NDEBUG)
            ,debugName(desc.debugName)
        #endif
        {
            auto dxVS = static_cast<D3D11Shader*>(desc.vertexShader);

            // Input Layout
            if(desc.vertexLayout.elementCount > 0){
                std::vector<D3D11_INPUT_ELEMENT_DESC> elements(desc.vertexLayout.elementCount);
                for(uint32_t i=0; i<desc.vertexLayout.elementCount; ++i){
                    const auto& src = desc.vertexLayout.elements[i];
                    auto& dst = elements[i];

                    dst.SemanticName = src.semanticName;
                    dst.SemanticIndex = src.semanticIndex;
                    dst.Format = convertTextureFormat(src.format);
                    dst.InputSlot = src.inputSlot;
                    dst.AlignedByteOffset = src.alignedByteOffset;
                    dst.InputSlotClass = src.classification == RHIInputClassification::PerVertex ?
                        D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA;
                    dst.InstanceDataStepRate = src.instanceDataStepRate;
                }
                if(FAILED(device->CreateInputLayout(
                    elements.data(), static_cast<UINT>(elements.size()),
                    dxVS->getBytecodePointer(), dxVS->getBytecodeSize(),
                    &il
                ))){
                    throw std::runtime_error("Failed to create ID3D11InputLayout");
                }
            }

            auto dxPS = static_cast<D3D11Shader*>(desc.pixelShader);
            vs = dxVS->getVS();
            ps = dxPS->getPS();
            vs->AddRef();
            ps->AddRef();

            // RasterizerState
            D3D11_RASTERIZER_DESC rsDesc{
                .FillMode = convertFillMode(desc.rasterizer.fillMode),
                .CullMode = convertCullMode(desc.rasterizer.cullMode),
                .FrontCounterClockwise = desc.rasterizer.frontCounterClockwise,
                .DepthBias             = desc.rasterizer.depthBias,
                .DepthBiasClamp        = desc.rasterizer.depthBiasClamp,
                .SlopeScaledDepthBias  = desc.rasterizer.slopeScaledDepthBias,
                .DepthClipEnable       = desc.rasterizer.depthClipEnable,
                .ScissorEnable         = FALSE,
                .MultisampleEnable     = desc.rasterizer.multisampleEnable,
                .AntialiasedLineEnable = desc.rasterizer.antialiasedLineEnable
            };
            if(FAILED(device->CreateRasterizerState(&rsDesc, &rs))){
                throw std::runtime_error("Failed to create ID3D11RasterizerState");
            }

            // DepthStencilState
            D3D11_DEPTH_STENCIL_DESC dsDesc{
                .DepthEnable = FALSE,
                .StencilEnable = FALSE
            };
            if(desc.depthStencil.has_value()){
                dsDesc.DepthEnable = TRUE;
                dsDesc.DepthWriteMask = desc.depthStencil->depthWriteEnable ? 
                    D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                dsDesc.DepthFunc = convertCompareFunc(desc.depthStencil->depthFunc);

                if(desc.depthStencil->stencil.has_value()){
                    const auto& stencil = desc.depthStencil->stencil.value();
                    dsDesc.StencilEnable = TRUE;
                    dsDesc.StencilReadMask = stencil.readMask;
                    dsDesc.StencilWriteMask = stencil.writeMask;
                    dsDesc.FrontFace = convertStencilOpDesc(stencil.frontFace);
                    dsDesc.BackFace = convertStencilOpDesc(stencil.backFace);
                }
            }
            if(FAILED(device->CreateDepthStencilState(&dsDesc, &dss))){
                throw std::runtime_error("Failed to create ID3D11DepthStencilState");
            }

            // BlendState
            D3D11_BLEND_DESC bsDesc{
                .AlphaToCoverageEnable = desc.blend.alphaToCoverageEnable,
                .IndependentBlendEnable = desc.blend.independentBlendEnable
            };
            for(int i=0; i<8; ++i){
                const auto& src = desc.blend.renderTargets[i];
                auto& dst = bsDesc.RenderTarget[i];

                dst.BlendEnable    = src.blendEnable;
                dst.SrcBlend       = convertBlendFactor(src.srcBlend);
                dst.DestBlend      = convertBlendFactor(src.dstBlend);
                dst.BlendOp        = convertBlendOp(src.blendOp);
                dst.SrcBlendAlpha  = convertBlendFactor(src.srcBlendAlpha);
                dst.DestBlendAlpha = convertBlendFactor(src.dstBlendAlpha);
                dst.BlendOpAlpha   = convertBlendOp(src.blendOpAlpha);
                dst.RenderTargetWriteMask = src.renderTargetWriteMask;
            }
            if(FAILED(device->CreateBlendState(&bsDesc, &bs))){
                throw std::runtime_error("Failed to create ID3D11BlendState");
            }
        }

        D3D11PipelineState(
            ID3D11Device* device,
            const RHIComputePipelineStateDesc& desc
        ){
            // TODO
        }

        ~D3D11PipelineState(){
            if(il != nullptr){
                il->Release();
                il = nullptr;
            }
            if(rs != nullptr){
                rs->Release();
                rs = nullptr;
            }
            if(dss != nullptr){
                dss->Release();
                dss = nullptr;
            }
            if(bs != nullptr){
                bs->Release();
                bs = nullptr;
            }
            if(vs != nullptr){
                vs->Release();
                vs = nullptr;
            }
            if(ps != nullptr){
                ps->Release();
                ps = nullptr;
            }
        }

        ID3D11InputLayout*       getIL () const{ return  il; }
        ID3D11RasterizerState*   getRS () const{ return  rs; }
        ID3D11DepthStencilState* getDSS() const{ return dss; }
        ID3D11BlendState*        getBS () const{ return  bs; }
        ID3D11VertexShader*      getVS () const{ return  vs; }
        ID3D11PixelShader*       getPS () const{ return  ps; }

        D3D11_PRIMITIVE_TOPOLOGY getTopology() const{
            return topology;
        }
    };
}