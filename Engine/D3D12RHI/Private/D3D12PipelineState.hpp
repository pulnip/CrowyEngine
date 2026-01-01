#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "D3D12Shader.hpp"
#include "D3D12Util.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12PipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    private:
        ComPtr<ID3D12PipelineState> pipelineState;
        ComPtr<ID3D12RootSignature> rootSignature;
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
        bool isCompute = false;

    public:
        D3D12PipelineState(
            ID3D12Device* device,
            const RHIGraphicsPipelineStateDesc& desc
        )
            : isCompute(false)
            , topology(desc.topology)
        {
            // Create root signature
            // Simple root signature: CBV(b0, b1), SRV(t0)
            D3D12_ROOT_PARAMETER rootParams[3] = {};

            // CBV for vertex shader (slot 1)
            rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParams[0].Descriptor.ShaderRegister = 1;
            rootParams[0].Descriptor.RegisterSpace = 0;
            rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

            // CBV for pixel shader (slot 2)
            rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParams[1].Descriptor.ShaderRegister = 2;
            rootParams[1].Descriptor.RegisterSpace = 0;
            rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // SRV table for pixel shader
            D3D12_DESCRIPTOR_RANGE srvRange = {};
            srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors = 1;
            srvRange.BaseShaderRegister = 0;
            srvRange.RegisterSpace = 0;
            srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
            rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;
            rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // Static sampler
            D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
            samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.MipLODBias = 0.0f;
            samplerDesc.MaxAnisotropy = 16;
            samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
            samplerDesc.MinLOD = 0.0f;
            samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
            samplerDesc.ShaderRegister = 0;
            samplerDesc.RegisterSpace = 0;
            samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
            rootSigDesc.NumParameters = 3;
            rootSigDesc.pParameters = rootParams;
            rootSigDesc.NumStaticSamplers = 1;
            rootSigDesc.pStaticSamplers = &samplerDesc;
            rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            if(FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))){
                throw std::runtime_error("Failed to serialize root signature");
            }

            if(FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))){
                throw std::runtime_error("Failed to create root signature");
            }

            // Create graphics pipeline state
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = rootSignature.Get();

            // Shaders
            auto vs = static_cast<D3D12Shader*>(desc.vertexShader);
            auto ps = static_cast<D3D12Shader*>(desc.pixelShader);

            if(vs) psoDesc.VS = vs->getBytecode();
            if(ps) psoDesc.PS = ps->getBytecode();

            // Input layout
            std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
            if(desc.vertexLayout.elementCount > 0){
                for(uint32_t i = 0; i < desc.vertexLayout.elementCount; ++i){
                    const auto& elem = desc.vertexLayout.elements[i];
                    D3D12_INPUT_ELEMENT_DESC d3dElem = {};
                    d3dElem.SemanticName = elem.semanticName;
                    d3dElem.SemanticIndex = elem.semanticIndex;
                    d3dElem.Format = convertTextureFormat(elem.format);
                    d3dElem.InputSlot = elem.inputSlot;
                    d3dElem.AlignedByteOffset = elem.alignedByteOffset;
                    d3dElem.InputSlotClass = (elem.classification == RHIInputClassification::PerVertex)
                        ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
                        : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                    d3dElem.InstanceDataStepRate = elem.instanceDataStepRate;
                    inputElements.push_back(d3dElem);
                }
            }

            psoDesc.InputLayout.pInputElementDescs = inputElements.data();
            psoDesc.InputLayout.NumElements = static_cast<UINT>(inputElements.size());

            // Rasterizer state
            psoDesc.RasterizerState.FillMode = convertFillMode(desc.rasterizer.fillMode);
            psoDesc.RasterizerState.CullMode = convertCullMode(desc.rasterizer.cullMode);
            psoDesc.RasterizerState.FrontCounterClockwise = desc.rasterizer.frontCounterClockwise;
            psoDesc.RasterizerState.DepthBias = desc.rasterizer.depthBias;
            psoDesc.RasterizerState.DepthBiasClamp = desc.rasterizer.depthBiasClamp;
            psoDesc.RasterizerState.SlopeScaledDepthBias = desc.rasterizer.slopeScaledDepthBias;
            psoDesc.RasterizerState.DepthClipEnable = desc.rasterizer.depthClipEnable;
            psoDesc.RasterizerState.MultisampleEnable = desc.rasterizer.multisampleEnable;
            psoDesc.RasterizerState.AntialiasedLineEnable = desc.rasterizer.antialiasedLineEnable;
            psoDesc.RasterizerState.ForcedSampleCount = 0;
            psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

            // Blend state
            psoDesc.BlendState.AlphaToCoverageEnable = desc.blend.alphaToCoverageEnable;
            psoDesc.BlendState.IndependentBlendEnable = desc.blend.independentBlendEnable;
            for(uint32_t i = 0; i < 8; ++i){
                const auto& rtBlend = desc.blend.renderTargets[i];
                auto& d3dRtBlend = psoDesc.BlendState.RenderTarget[i];

                d3dRtBlend.BlendEnable = rtBlend.blendEnable;
                d3dRtBlend.SrcBlend = convertBlend(rtBlend.srcBlend);
                d3dRtBlend.DestBlend = convertBlend(rtBlend.dstBlend);
                d3dRtBlend.BlendOp = convertBlendOp(rtBlend.blendOp);
                d3dRtBlend.SrcBlendAlpha = convertBlend(rtBlend.srcBlendAlpha);
                d3dRtBlend.DestBlendAlpha = convertBlend(rtBlend.dstBlendAlpha);
                d3dRtBlend.BlendOpAlpha = convertBlendOp(rtBlend.blendOpAlpha);
                d3dRtBlend.RenderTargetWriteMask = rtBlend.renderTargetWriteMask;
            }

            // Depth stencil state
            psoDesc.DepthStencilState.DepthEnable = desc.depthStencil.depthEnable;
            psoDesc.DepthStencilState.DepthWriteMask = desc.depthStencil.depthWriteEnable
                ? D3D12_DEPTH_WRITE_MASK_ALL
                : D3D12_DEPTH_WRITE_MASK_ZERO;
            psoDesc.DepthStencilState.DepthFunc = convertComparisonFunc(desc.depthStencil.depthFunc);
            psoDesc.DepthStencilState.StencilEnable = desc.depthStencil.stencilEnable;
            psoDesc.DepthStencilState.StencilReadMask = desc.depthStencil.stencilReadMask;
            psoDesc.DepthStencilState.StencilWriteMask = desc.depthStencil.stencilWriteMask;

            // Render target formats
            psoDesc.NumRenderTargets = desc.renderTargetCount;
            for(uint32_t i = 0; i < desc.renderTargetCount; ++i){
                psoDesc.RTVFormats[i] = convertTextureFormat(desc.renderTargetFormats[i]);
            }

            // Depth stencil format
            if(desc.depthStencilFormat != RHITextureFormat::Unknown){
                psoDesc.DSVFormat = convertTextureFormat(desc.depthStencilFormat);
            }

            // Sample desc
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.SampleDesc.Count = 1;
            psoDesc.SampleDesc.Quality = 0;

            // Primitive topology
            psoDesc.PrimitiveTopologyType = convertTopologyType(desc.topology);

            if(FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))){
                throw std::runtime_error("Failed to create graphics pipeline state");
            }
        }

        D3D12PipelineState(
            ID3D12Device* device,
            const RHIComputePipelineStateDesc& desc
        )
            : isCompute(true)
        {
            // Simple root signature for compute
            D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
            rootSigDesc.NumParameters = 0;
            rootSigDesc.pParameters = nullptr;
            rootSigDesc.NumStaticSamplers = 0;
            rootSigDesc.pStaticSamplers = nullptr;
            rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            if(FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))){
                throw std::runtime_error("Failed to serialize root signature");
            }

            if(FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))){
                throw std::runtime_error("Failed to create root signature");
            }

            auto cs = static_cast<D3D12Shader*>(desc.computeShader);
            if(!cs){
                throw std::runtime_error("Compute shader is null");
            }

            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = rootSignature.Get();
            psoDesc.CS = cs->getBytecode();

            if(FAILED(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))){
                throw std::runtime_error("Failed to create compute pipeline state");
            }
        }

        ~D3D12PipelineState(){
        }

        ID3D12PipelineState* get() const{
            return pipelineState.Get();
        }

        ID3D12RootSignature* getRootSignature() const{
            return rootSignature.Get();
        }

        RHIPrimitiveTopology getTopology() const{
            return topology;
        }

        bool isComputePipeline() const{
            return isCompute;
        }
    };
}