#pragma once

#include <string>
#include <d3d11.h>
#include "string.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif
#include "D3D11Definitions.hpp"
#include "D3D11Util.hpp"

namespace Crowy
{
    inline auto convertFillMode(RHIFillMode mode){
        using enum RHIFillMode;

        switch(mode){
        case Solid:     return D3D11_FILL_SOLID;
        case Wireframe: return D3D11_FILL_WIREFRAME;
        default:
            std::unreachable();
        }
    }

    inline auto convertCullMode(RHICullMode mode){
        using enum RHICullMode;

        switch(mode){
        case CullNone: return D3D11_CULL_NONE;
        case Front:    return D3D11_CULL_FRONT;
        case Back:     return D3D11_CULL_BACK;
        default:
            std::unreachable();
        }
    }

    inline auto convertStencilOp(RHIStencilOp op){
        using enum RHIStencilOp;

        switch (op){
        case Keep:     return D3D11_STENCIL_OP_KEEP;
        case Zero:     return D3D11_STENCIL_OP_ZERO;
        case Replace:  return D3D11_STENCIL_OP_REPLACE;
        case IncrSat:  return D3D11_STENCIL_OP_INCR_SAT;
        case DecrSat:  return D3D11_STENCIL_OP_DECR_SAT;
        case Invert:   return D3D11_STENCIL_OP_INVERT;
        case IncrWrap: return D3D11_STENCIL_OP_INCR;
        case DecrWrap: return D3D11_STENCIL_OP_DECR;
        default:
            std::unreachable();
        }
    }

    inline auto convertStencilOpDesc(const RHIStencilOpDesc& desc) {
        return D3D11_DEPTH_STENCILOP_DESC{
            .StencilFailOp = convertStencilOp(desc.stencilFailOp),
            .StencilDepthFailOp = convertStencilOp(desc.depthFailOp),
            .StencilPassOp = convertStencilOp(desc.passOp),
            .StencilFunc = convertCompareFunc(desc.func),
        };
    }

    inline auto convertBlendFactor(RHIBlend blend){
        using enum RHIBlend;

        switch (blend) {
        case Zero:          return D3D11_BLEND_ZERO;
        case One:           return D3D11_BLEND_ONE;
        case SrcColor:      return D3D11_BLEND_SRC_COLOR;
        case InvSrcColor:   return D3D11_BLEND_INV_SRC_COLOR;
        case SrcAlpha:      return D3D11_BLEND_SRC_ALPHA;
        case InvSrcAlpha:   return D3D11_BLEND_INV_SRC_ALPHA;
        case DestAlpha:     return D3D11_BLEND_DEST_ALPHA;
        case InvDestAlpha:  return D3D11_BLEND_INV_DEST_ALPHA;
        case DestColor:     return D3D11_BLEND_DEST_COLOR;
        case InvDestColor:  return D3D11_BLEND_INV_DEST_COLOR;
        case SrcAlphaSat:   return D3D11_BLEND_SRC_ALPHA_SAT;
        case BlendFactor:   return D3D11_BLEND_BLEND_FACTOR;
        case InvBlendFactor: return D3D11_BLEND_INV_BLEND_FACTOR;
        default:                      return D3D11_BLEND_ONE;
        }
    }

    inline auto convertBlendOp(RHIBlendOp op){
        using enum RHIBlendOp;

        switch (op) {
        case Add:             return D3D11_BLEND_OP_ADD;
        case Subtract:        return D3D11_BLEND_OP_SUBTRACT;
        case ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case Min:             return D3D11_BLEND_OP_MIN;
        case Max:             return D3D11_BLEND_OP_MAX;
        default:                          return D3D11_BLEND_OP_ADD;
        }
    }

    inline auto convertTopology(RHIPrimitiveTopology primitiveTopology){
        using enum RHIPrimitiveTopology;

        switch(primitiveTopology){
        case PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            std::unreachable();
        }
    }

    using Blob = ID3DBlob;
	using BlobRAII = RAII<Blob>;

	struct CompiledShader {
		std::vector<uint8_t> bytecode;

		CompiledShader(
			const std::filesystem::path& path,
			const std::string& entryPoint,
			const char* target
		) {
			auto ext = std::filesystem::path(path).extension().string();

			if (ext == ".cso" || ext == ".dxbc" || ext == ".dxil") {
				bytecode = read_file_as_binary(path);
			}
			else if(ext != ".hlsl") {
				throw std::runtime_error("Unsupported shader file extension: " + ext);
			}

			UINT compileFlags = 0;
#if defined(_DEBUG) || !defined(NDEBUG)
			compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
			BlobRAII shaderBlob, errorBlob;

			if (FAILED(D3DCompileFromFile(
				path.c_str(),
				nullptr,
				D3D_COMPILE_STANDARD_FILE_INCLUDE,
				entryPoint.c_str(),
				target,
				compileFlags,
				0,
				&shaderBlob,
				&errorBlob
			))) {
				std::string errorMsg = "HLSL compile failed";
				if (errorBlob) {
					errorMsg += ": ";
					errorMsg += static_cast<const char*>(errorBlob->GetBufferPointer());
				}
				throw std::runtime_error(errorMsg);
			}

			bytecode.resize(shaderBlob->GetBufferSize());
			memcpy(bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

			if (bytecode.empty()) {
				throw std::runtime_error("Shader bytecode is empty");
			}
		}

		const void* getBytecode() const {
			return bytecode.data();
		}
		SIZE_T getBytecodeLength() const {
			return bytecode.size();
		}
	};

    class D3D11GraphicsPipelineState
#ifndef USE_STATIC_RHI
        : public RHIGraphicsPipelineState
#endif
    {
    private:
        InputLayoutRAII inputLayout = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        VertexShaderRAII vertexShader = nullptr;
        RasterizerStateRAII rasterizerState = nullptr;
        PixelShaderRAII pixelShader = nullptr;
        DepthStencilStateRAII depthStencilState = nullptr;
        BlendStateRAII blendState = nullptr;

        RHIGraphicsBindingInfo bindingInfo;

    #if defined(_DEBUG) || !defined(NDEBUG)
        const std::string debugName;
    #endif

    public:
        D3D11GraphicsPipelineState(
            Device& device,
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        )
        #if defined(_DEBUG) || !defined(NDEBUG)
            : debugName(name)
        #endif
        {
            auto vsBytecode = CompiledShader(desc.vertexShaderPath, desc.vertexShaderEntryPoint, "vs_5_0");
            if(FAILED(device.CreateVertexShader(
                vsBytecode.getBytecode(),
                vsBytecode.getBytecodeLength(),
                nullptr,
                &vertexShader
            ))) {
                throw std::runtime_error("Failed to create vertex shader");
            }

            // Input Layout
            if(desc.vertexLayout.has_value()){
                const auto& vertexLayout = desc.vertexLayout.value();

                std::vector<D3D11_INPUT_ELEMENT_DESC> elements(vertexLayout.size());
                for(uint32_t i=0; i<vertexLayout.size(); ++i){
                    const auto& src = vertexLayout[i];
                    auto& dst = elements[i];

                    dst.SemanticName = src.semanticName;
                    dst.SemanticIndex = src.semanticIndex;
                    dst.Format = convertPixelFormat(src.format);
                    dst.InputSlot = src.inputSlot;
                    dst.AlignedByteOffset = src.alignedByteOffset;
                    dst.InputSlotClass = src.classification == RHIInputClassification::PerVertex ?
                        D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA;
                    dst.InstanceDataStepRate = src.instanceDataStepRate;
                }
                if(FAILED(device.CreateInputLayout(
                    elements.data(), static_cast<UINT>(elements.size()),
                    vsBytecode.getBytecode(), vsBytecode.getBytecodeLength(),
                    &inputLayout
                ))){
                    throw std::runtime_error("Failed to create ID3D11InputLayout");
                }
            }

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
            if(FAILED(device.CreateRasterizerState(&rsDesc, &rasterizerState))){
                throw std::runtime_error("Failed to create ID3D11RasterizerState");
            }

            auto psBytecode = CompiledShader(desc.fragmentShaderPath, desc.fragmentShaderEntryPoint, "ps_5_0");
            if(FAILED(device.CreatePixelShader(
                psBytecode.getBytecode(),
                psBytecode.getBytecodeLength(),
                nullptr,
                &pixelShader
            ))){
                throw std::runtime_error("Failed to create pixel shader");
            }

            // DepthStencilState
            D3D11_DEPTH_STENCIL_DESC dsDesc{
                .DepthEnable = FALSE,
                .StencilEnable = FALSE
            };
            if(desc.depthStencil.has_value()){
                const auto& depthStencil = desc.depthStencil.value();

                dsDesc.DepthEnable = TRUE;
                dsDesc.DepthWriteMask = depthStencil.depthWriteEnable ? 
                    D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                dsDesc.DepthFunc = convertCompareFunc(depthStencil.depthFunc);

                if(depthStencil.stencil.has_value()){
                    const auto& stencil = depthStencil.stencil.value();

                    dsDesc.StencilEnable = TRUE;
                    dsDesc.StencilReadMask = stencil.readMask;
                    dsDesc.StencilWriteMask = stencil.writeMask;
                    dsDesc.FrontFace = convertStencilOpDesc(stencil.frontFace);
                    dsDesc.BackFace = convertStencilOpDesc(stencil.backFace);
                }
            }
            if(FAILED(device.CreateDepthStencilState(&dsDesc, &depthStencilState))){
                throw std::runtime_error("Failed to create ID3D11DepthStencilState");
            }

            // BlendState
            if(desc.blend.has_value()){
                const auto& blend = desc.blend.value();

                D3D11_BLEND_DESC bsDesc{
                    .AlphaToCoverageEnable = blend.alphaToCoverageEnable,
                    .IndependentBlendEnable = blend.independentBlendEnable
                };
                for(int i=0; i<8; ++i){
                    const auto& src = blend.renderTargets[i];
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
                if(FAILED(device.CreateBlendState(&bsDesc, &blendState))){
                    throw std::runtime_error("Failed to create ID3D11BlendState");
                }
            }
        }

        ~D3D11GraphicsPipelineState() = default;

        const RHIGraphicsBindingInfo& getInfo() const RHI_OVERRIDE{
            return bindingInfo;
        }

        void bind(DeviceContext& ctx) const{
            if (inputLayout != nullptr) {
                ctx.IASetInputLayout(inputLayout.Get());
                ctx.IASetPrimitiveTopology(primitiveTopology);
            }

            ctx.VSSetShader(vertexShader.Get(), nullptr, 0);
            ctx.RSSetState(rasterizerState.Get());
            ctx.PSSetShader(pixelShader.Get(), nullptr, 0);

            if (depthStencilState != nullptr) {
                ctx.OMSetDepthStencilState(depthStencilState.Get(), 0);
            }
            if (blendState != nullptr) {
                ctx.OMSetBlendState(blendState.Get(), nullptr, 0xFFFFFFFF);
            }
        }
    };

    class D3D11ComputePipelineState
#ifndef USE_STATIC_RHI
        : public RHIComputePipelineState
#endif
    {
    private:
        RHIComputeBindingInfo bindingInfo;

    #if defined(_DEBUG) || !defined(NDEBUG)
        const std::string debugName;
    #endif

    public:
        D3D11ComputePipelineState(
            ID3D11Device& device,
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        )
        #if defined(_DEBUG) || !defined(NDEBUG)
            : debugName(name)
        #endif
        {
            // TODO
        }

        ~D3D11ComputePipelineState(){
        }

        const RHIComputeBindingInfo& getInfo() const RHI_OVERRIDE{
            return bindingInfo;
        }
    };
}