#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif
#include "D3D12Shader.hpp"
#include "D3D12Util.hpp"

namespace Crowy
{
    static D3D12_FILL_MODE convert(RHIFillMode mode){
        switch(mode){
        case RHIFillMode::Solid:     return D3D12_FILL_MODE_SOLID;
        case RHIFillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
        default:
            std::unreachable();
        }
    }

    static D3D12_CULL_MODE convert(RHICullMode mode){
        switch(mode){
        case RHICullMode::CullNone: return D3D12_CULL_MODE_NONE;
        case RHICullMode::Front:    return D3D12_CULL_MODE_FRONT;
        case RHICullMode::Back:     return D3D12_CULL_MODE_BACK;
        default:
            std::unreachable();
        }
    }

    static D3D12_STENCIL_OP convert(RHIStencilOp op){
        switch (op){
        case RHIStencilOp::Keep:     return D3D12_STENCIL_OP_KEEP;
        case RHIStencilOp::Zero:     return D3D12_STENCIL_OP_ZERO;
        case RHIStencilOp::Replace:  return D3D12_STENCIL_OP_REPLACE;
        case RHIStencilOp::IncrSat:  return D3D12_STENCIL_OP_INCR_SAT;
        case RHIStencilOp::DecrSat:  return D3D12_STENCIL_OP_DECR_SAT;
        case RHIStencilOp::Invert:   return D3D12_STENCIL_OP_INVERT;
        case RHIStencilOp::IncrWrap: return D3D12_STENCIL_OP_INCR;
        case RHIStencilOp::DecrWrap: return D3D12_STENCIL_OP_DECR;
        default:
            std::unreachable();
        }
    }

    static auto convert(const RHIStencilOpDesc& desc) {
        return D3D12_DEPTH_STENCILOP_DESC{
            .StencilFailOp = convert(desc.stencilFailOp),
            .StencilDepthFailOp = convert(desc.depthFailOp),
            .StencilPassOp = convert(desc.passOp),
            .StencilFunc = convert(desc.func),
        };
    }

    static D3D12_BLEND convert(RHIBlend blend){
        switch (blend) {
        case RHIBlend::Zero:           return D3D12_BLEND_ZERO;
        case RHIBlend::One:            return D3D12_BLEND_ONE;
        case RHIBlend::SrcColor:       return D3D12_BLEND_SRC_COLOR;
        case RHIBlend::InvSrcColor:    return D3D12_BLEND_INV_SRC_COLOR;
        case RHIBlend::SrcAlpha:       return D3D12_BLEND_SRC_ALPHA;
        case RHIBlend::InvSrcAlpha:    return D3D12_BLEND_INV_SRC_ALPHA;
        case RHIBlend::DestAlpha:      return D3D12_BLEND_DEST_ALPHA;
        case RHIBlend::InvDestAlpha:   return D3D12_BLEND_INV_DEST_ALPHA;
        case RHIBlend::DestColor:      return D3D12_BLEND_DEST_COLOR;
        case RHIBlend::InvDestColor:   return D3D12_BLEND_INV_DEST_COLOR;
        case RHIBlend::SrcAlphaSat:    return D3D12_BLEND_SRC_ALPHA_SAT;
        case RHIBlend::BlendFactor:    return D3D12_BLEND_BLEND_FACTOR;
        case RHIBlend::InvBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            std::unreachable();
        }
    }

    static D3D12_BLEND_OP convert(RHIBlendOp op){
        switch (op) {
        case RHIBlendOp::Add:             return D3D12_BLEND_OP_ADD;
        case RHIBlendOp::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
        case RHIBlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case RHIBlendOp::Min:             return D3D12_BLEND_OP_MIN;
        case RHIBlendOp::Max:             return D3D12_BLEND_OP_MAX;
        default:
            std::unreachable();
        }
    }

    static D3D12_PRIMITIVE_TOPOLOGY_TYPE convertTopologyType(RHIPrimitiveTopology topology){
        switch(topology){
        case RHIPrimitiveTopology::PointList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case RHIPrimitiveTopology::LineList:      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case RHIPrimitiveTopology::LineStrip:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case RHIPrimitiveTopology::TriangleList:  return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case RHIPrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            std::unreachable();
        }
    }

    struct ShaderInfo{
        using BindPoint = UINT;
        struct Slot{
            std::string name;
            UINT bindCount = 1;
        };
        std::unordered_map<BindPoint, Slot> cbvSlots;
        std::map<BindPoint, Slot> srvSlots;
    };

    static auto getShaderInfo(D3D12Shader* shader){
        Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;

        if(FAILED(D3DReflect(
            shader->getBytecodePointer(),
            shader->getBytecodeSize(),
            IID_PPV_ARGS(&reflection)
        ))){
            throw std::runtime_error("D3DReflect Failed");
        }

        D3D12_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);
        ShaderInfo info;

        for(UINT i=0; i<shaderDesc.BoundResources; ++i){
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            // TODO. currently not support register space
            CROWY_ASSERT(bindDesc.Space == 0);

            switch(bindDesc.Type){
            case D3D_SIT_CBUFFER:
                info.cbvSlots[bindDesc.BindPoint] = ShaderInfo::Slot{
                    .name = bindDesc.Name
                };
                break;
            case D3D_SIT_TEXTURE:
                info.srvSlots[bindDesc.BindPoint] = ShaderInfo::Slot{
                    .name = bindDesc.Name,
                    .bindCount = bindDesc.BindCount
                };
                break;
            // ...
            }
        }

        return info;
    }

    static auto mergeCBVParams(
        const std::unordered_map<UINT, ShaderInfo::Slot>& vscbvSlots,
        const std::unordered_map<UINT, ShaderInfo::Slot>& pscbvSlots
    ){
        std::unordered_map<UINT, ShaderInfo::Slot> _pscbvSlots = pscbvSlots;
        std::vector<D3D12_ROOT_PARAMETER> params;

        for(const auto& [bindPoint, vscbv]: vscbvSlots){
            auto it = _pscbvSlots.find(bindPoint);

            if(it != _pscbvSlots.end()){
                const auto& pscbv = it->second;

                // shared cbv
                if(vscbv.name == pscbv.name){
                    params.push_back({
                        // TODO. use cbv.name prefix?
                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                        .Descriptor = {
                            .ShaderRegister = bindPoint,
                            .RegisterSpace = 0
                        },
                        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
                    });
                }
                else{
                    // different cbv
                    params.push_back({
                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                        .Descriptor = {
                            .ShaderRegister = bindPoint,
                            .RegisterSpace = 0
                        },
                        .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
                    });
                    params.push_back({
                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                        .Descriptor = {
                            .ShaderRegister = bindPoint,
                            .RegisterSpace = 0
                        },
                        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
                    });
                }

                // erase that have been processed
                _pscbvSlots.erase(it);
            }
            else{
                // BindPoint only used by vertex shader
                params.push_back({
                    .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                    .Descriptor = {
                        .ShaderRegister = bindPoint,
                        .RegisterSpace = 0
                    },
                    .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
                });
            }
        }

        // BindPoint only used by pixel shader
        for(const auto& [bindPoint, pscbv]: _pscbvSlots){
            params.push_back({
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                .Descriptor = {
                    .ShaderRegister = bindPoint,
                    .RegisterSpace = 0
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
            });
        }

        return params;
    }

    static auto mergeSRVRanges(
        const std::map<UINT, ShaderInfo::Slot>& vssrvSlots,
        const std::map<UINT, ShaderInfo::Slot>& pssrvSlots
    ){
        std::map<UINT, ShaderInfo::Slot> _pssrvSlots = pssrvSlots;

        UINT vsRangeStart = UINT_MAX, vsRangeCount = 1;
        std::vector<D3D12_DESCRIPTOR_RANGE> vssrvRanges;
        UINT psRangeStart = UINT_MAX, psRangeCount = 1;
        std::vector<D3D12_DESCRIPTOR_RANGE> pssrvRanges;
        // shared
        UINT   rangeStart = UINT_MAX,   rangeCount = 1;
        std::vector<D3D12_DESCRIPTOR_RANGE>   srvRanges;

        for(const auto& [bindPoint, vssrv]: vssrvSlots){
            auto it = _pssrvSlots.find(bindPoint);

            if(it != _pssrvSlots.end()){
                const auto& pssrv = it->second;

                // shared srv
                if(vssrv.name == pssrv.name){
                    if(rangeStart == UINT_MAX)
                        rangeStart = bindPoint;

                    if(bindPoint == rangeStart + rangeCount){
                        ++rangeCount;
                    }
                    else{
                        srvRanges.push_back({
                            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                            .NumDescriptors = rangeCount,
                            .BaseShaderRegister = rangeStart,
                            .RegisterSpace = 0,
                            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                        });

                        rangeStart = bindPoint;
                        rangeCount = 1;
                    }
                }
                else{
                    // different srv
                    if(vsRangeStart == UINT_MAX)
                        vsRangeStart = bindPoint;
                    if(psRangeStart == UINT_MAX)
                        psRangeStart = bindPoint;

                    if(bindPoint == vsRangeStart + vsRangeCount){
                        ++vsRangeCount;
                    }
                    else{
                        vssrvRanges.push_back({
                            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                            .NumDescriptors = vsRangeCount,
                            .BaseShaderRegister = vsRangeStart,
                            .RegisterSpace = 0,
                            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                        });

                        vsRangeStart = bindPoint;
                        vsRangeCount = 1;
                    }

                    if(bindPoint == psRangeStart + psRangeCount){
                        ++psRangeCount;
                    }
                    else{
                        pssrvRanges.push_back({
                            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                            .NumDescriptors = psRangeCount,
                            .BaseShaderRegister = psRangeStart,
                            .RegisterSpace = 0,
                            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                        });

                        psRangeStart = bindPoint;
                        psRangeCount = 1;
                    }
                }

                // erase that have been processed
                _pssrvSlots.erase(it);
            }
            else{
                // BindPoint only used by vertex shader
                if(vsRangeStart == UINT_MAX)
                    vsRangeStart = bindPoint;

                if(bindPoint == vsRangeStart + vsRangeCount){
                    ++vsRangeCount;
                }
                else{
                    vssrvRanges.push_back({
                        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                        .NumDescriptors = vsRangeCount,
                        .BaseShaderRegister = vsRangeStart,
                        .RegisterSpace = 0,
                        .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                    });

                    vsRangeStart = bindPoint;
                    vsRangeCount = 1;
                }
            }
        }

        // final vssrv range
        vssrvRanges.push_back({
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = vsRangeCount,
            .BaseShaderRegister = vsRangeStart,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        });

        // BindPoint only used by pixel shader
        for(const auto& [bindPoint, pscbv]: _pssrvSlots){
            if(psRangeStart == UINT_MAX)
                psRangeStart = bindPoint;

            if(bindPoint == psRangeStart + psRangeCount){
                ++psRangeCount;
            }
            else{
                pssrvRanges.push_back({
                    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                    .NumDescriptors = psRangeCount,
                    .BaseShaderRegister = psRangeStart,
                    .RegisterSpace = 0,
                    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                });

                psRangeStart = bindPoint;
                psRangeCount = 1;
            }
        }

        // final pssrv range
        pssrvRanges.push_back({
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = psRangeCount,
            .BaseShaderRegister = psRangeStart,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        });

        return std::make_tuple(vssrvRanges, pssrvRanges, srvRanges);
    }

    static auto createRootSignature(ID3D12Device* device, D3D12Shader* vs, D3D12Shader* ps){
        auto vsInfo = getShaderInfo(vs);
        auto psInfo = getShaderInfo(ps);

        auto params = mergeCBVParams(vsInfo.cbvSlots, psInfo.cbvSlots);
        auto [vssrvRanges, pssrvRanges, srvRanges] = mergeSRVRanges(
            vsInfo.srvSlots, psInfo.srvSlots
        );

        if(!vssrvRanges.empty()){
            params.push_back({
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                .DescriptorTable = {
                    .NumDescriptorRanges = static_cast<UINT>(vssrvRanges.size()),
                    .pDescriptorRanges = vssrvRanges.data()
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
            });
        }
        if(!pssrvRanges.empty()){
            params.push_back({
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                .DescriptorTable = {
                    .NumDescriptorRanges = static_cast<UINT>(pssrvRanges.size()),
                    .pDescriptorRanges = pssrvRanges.data()
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
            });
        }
        if(!srvRanges.empty()){
            params.push_back({
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                .DescriptorTable = {
                    .NumDescriptorRanges = static_cast<UINT>(srvRanges.size()),
                    .pDescriptorRanges = srvRanges.data()
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
            });
        }

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc{
            .NumParameters = static_cast<UINT>(params.size()),
            .pParameters = params.data(),
            .NumStaticSamplers = 0,
            .pStaticSamplers = nullptr,
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        };

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        if(FAILED(D3D12SerializeRootSignature(
            &rootSigDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error
        ))){
            throw std::runtime_error("Failed to serialize root signature");
        }

        ID3D12RootSignature* rootSignature;

        if(FAILED(device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)
        ))){
            throw std::runtime_error("Failed to create root signature");
        }

        return rootSignature;
    }

    class D3D12PipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    private:
        ID3D12PipelineState* pipeline = nullptr;
        ID3D12RootSignature* rootSignature;

        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
        bool isCompute = false;

    public:
        D3D12PipelineState(
            ID3D12Device* device,
            const RHIGraphicsPipelineStateDesc& desc
        )
            : topology(desc.topology)
        {
            auto vs = static_cast<D3D12Shader*>(desc.vertexShader);
            auto ps = static_cast<D3D12Shader*>(desc.pixelShader);

            rootSignature = createRootSignature(device, vs, ps);

            // Input layout
            std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
            if(desc.vertexLayout.elementCount > 0){
                elements.reserve(desc.vertexLayout.elementCount);

                for(uint32_t i=0; i<desc.vertexLayout.elementCount; ++i){
                    const auto& src = desc.vertexLayout.elements[i];

                    elements.push_back({
                        .SemanticName = src.semanticName,
                        .SemanticIndex = src.semanticIndex,
                        .Format = convert(src.format),
                        .InputSlot = src.inputSlot,
                        .AlignedByteOffset = src.alignedByteOffset,
                        .InputSlotClass = src.classification == RHIInputClassification::PerVertex ?
                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA :
                            D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                        .InstanceDataStepRate = src.instanceDataStepRate
                    });
                }
            }

            // RasterizerState
            D3D12_RASTERIZER_DESC rsDesc{
                .FillMode = convert(desc.rasterizer.fillMode),
                .CullMode = convert(desc.rasterizer.cullMode),
                .FrontCounterClockwise = desc.rasterizer.frontCounterClockwise,
                .DepthBias             = desc.rasterizer.depthBias,
                .DepthBiasClamp        = desc.rasterizer.depthBiasClamp,
                .SlopeScaledDepthBias  = desc.rasterizer.slopeScaledDepthBias,
                .DepthClipEnable       = desc.rasterizer.depthClipEnable,
                .MultisampleEnable     = desc.rasterizer.multisampleEnable,
                .AntialiasedLineEnable = desc.rasterizer.antialiasedLineEnable,
                .ForcedSampleCount     = 0,
                .ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
            };

            // BlendState
            D3D12_BLEND_DESC bsDesc{
                .AlphaToCoverageEnable = desc.blend.alphaToCoverageEnable,
                .IndependentBlendEnable = desc.blend.independentBlendEnable
            };
            for(int i=0; i<8; ++i){
                const auto& src = desc.blend.renderTargets[i];
                auto& dst = bsDesc.RenderTarget[i];

                dst.BlendEnable    = src.blendEnable;
                dst.SrcBlend       = convert(src.srcBlend);
                dst.DestBlend      = convert(src.dstBlend);
                dst.BlendOp        = convert(src.blendOp);
                dst.SrcBlendAlpha  = convert(src.srcBlendAlpha);
                dst.DestBlendAlpha = convert(src.dstBlendAlpha);
                dst.BlendOpAlpha   = convert(src.blendOpAlpha);
                dst.RenderTargetWriteMask = src.renderTargetWriteMask;
            }

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{
                .pRootSignature = rootSignature,
                .VS = vs->getBytecode(),
                .PS = ps->getBytecode(),
                .BlendState = bsDesc,
                .SampleMask = UINT_MAX,
                .RasterizerState = rsDesc,
                .InputLayout = {
                    .pInputElementDescs = elements.data(),
                    .NumElements = static_cast<UINT>(elements.size())
                },
                .PrimitiveTopologyType = convertTopologyType(desc.topology),
                .NumRenderTargets = desc.renderTargetCount,
                .DSVFormat = DXGI_FORMAT_UNKNOWN,
                .SampleDesc = {1, 0}
            };

            // DepthStencilState
            D3D12_DEPTH_STENCIL_DESC dsDesc{
                .DepthEnable = FALSE,
                .StencilEnable = FALSE
            };
            if(desc.depthStencil.has_value()){
                pipelineDesc.DSVFormat = convert(desc.depthStencil->format);

                dsDesc.DepthEnable = TRUE;
                dsDesc.DepthWriteMask = desc.depthStencil->depthWriteEnable ? 
                    D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
                dsDesc.DepthFunc = convert(desc.depthStencil->depthFunc);

                if(desc.depthStencil->stencil.has_value()){
                    const auto& stencil = desc.depthStencil->stencil.value();
                    dsDesc.StencilEnable = TRUE;
                    dsDesc.StencilReadMask = stencil.readMask;
                    dsDesc.StencilWriteMask = stencil.writeMask;
                    dsDesc.FrontFace = convert(stencil.frontFace);
                    dsDesc.BackFace = convert(stencil.backFace);
                }
            }

            // Render target formats
            for(uint32_t i = 0; i < desc.renderTargetCount; ++i){
                pipelineDesc.RTVFormats[i] = convert(desc.renderTargetFormats[i]);
            }

            if(FAILED(device->CreateGraphicsPipelineState(
                &pipelineDesc,
                IID_PPV_ARGS(&pipeline)
            ))){
                throw std::runtime_error("Failed to create graphics pipeline state");
            }
        }

        D3D12PipelineState(
            ID3D12Device* device,
            const RHIComputePipelineStateDesc& desc
        ){
            // TODO
            throw std::runtime_error("Unimplemented");
        }

        ~D3D12PipelineState(){
            if(rootSignature != nullptr){
                rootSignature->Release();
                rootSignature = nullptr;
            }
            if(pipeline != nullptr){
                pipeline->Release();
                pipeline = nullptr;
            }
        }

        auto getPipeline() const{ return pipeline; }
        auto getRootSignature() const{ return rootSignature; }

        auto getTopology() const{ return topology; }
        bool isComputePipeline() const{ return isCompute; }
    };
}