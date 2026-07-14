#include <d3dx12/d3dx12_root_signature.h>
#include "DX12PipelineState.hpp"
#include "DX12Definitions.hpp"
#include "DX12Util.hpp"
#include "RHIShader.hpp"

namespace{
    auto convert(Crowy::RHIFillMode mode){
        using enum Crowy::RHIFillMode;

        switch(mode){
        case Solid:     return D3D12_FILL_MODE_SOLID;
        case Wireframe: return D3D12_FILL_MODE_WIREFRAME;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHICullMode mode){
        using enum Crowy::RHICullMode;

        switch(mode){
        case None:     return D3D12_CULL_MODE_NONE;
        case Front:    return D3D12_CULL_MODE_FRONT;
        case Back:     return D3D12_CULL_MODE_BACK;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIStencilOp op){
        using enum Crowy::RHIStencilOp;

        switch (op){
        case Keep:     return D3D12_STENCIL_OP_KEEP;
        case Zero:     return D3D12_STENCIL_OP_ZERO;
        case Replace:  return D3D12_STENCIL_OP_REPLACE;
        case IncrSat:  return D3D12_STENCIL_OP_INCR_SAT;
        case DecrSat:  return D3D12_STENCIL_OP_DECR_SAT;
        case Invert:   return D3D12_STENCIL_OP_INVERT;
        case IncrWrap: return D3D12_STENCIL_OP_INCR;
        case DecrWrap: return D3D12_STENCIL_OP_DECR;
        default:
            std::unreachable();
        }
    }

    auto convert(const Crowy::RHIStencilOpDesc& desc) {
        return D3D12_DEPTH_STENCILOP_DESC{
            .StencilFailOp = convert(desc.stencilFailOp),
            .StencilDepthFailOp = convert(desc.depthFailOp),
            .StencilPassOp = convert(desc.passOp),
            .StencilFunc = convert(desc.func),
        };
    }

    auto convert(Crowy::RHIBlend blend){
        using enum Crowy::RHIBlend;

        switch (blend) {
        case Zero:           return D3D12_BLEND_ZERO;
        case One:            return D3D12_BLEND_ONE;
        case SrcColor:       return D3D12_BLEND_SRC_COLOR;
        case InvSrcColor:    return D3D12_BLEND_INV_SRC_COLOR;
        case SrcAlpha:       return D3D12_BLEND_SRC_ALPHA;
        case InvSrcAlpha:    return D3D12_BLEND_INV_SRC_ALPHA;
        case DstAlpha:       return D3D12_BLEND_DEST_ALPHA;
        case InvDstAlpha:    return D3D12_BLEND_INV_DEST_ALPHA;
        case DstColor:       return D3D12_BLEND_DEST_COLOR;
        case InvDstColor:    return D3D12_BLEND_INV_DEST_COLOR;
        case SrcAlphaSat:    return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor:    return D3D12_BLEND_BLEND_FACTOR;
        case InvBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIBlendOp op){
        using enum Crowy::RHIBlendOp;

        switch (op) {
        case Add:             return D3D12_BLEND_OP_ADD;
        case Subtract:        return D3D12_BLEND_OP_SUBTRACT;
        case ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case Min:             return D3D12_BLEND_OP_MIN;
        case Max:             return D3D12_BLEND_OP_MAX;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIPrimitiveTopology primitiveTopology){
        using enum Crowy::RHIPrimitiveTopology;

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

    auto topologyType(D3D_PRIMITIVE_TOPOLOGY topology){
        switch(topology){
        case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case D3D_PRIMITIVE_TOPOLOGY_LINELIST:      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:  return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            std::unreachable();
        }
    }

    enum class StageVisibility{
        None = 0,
        VS = 1 << 0,
        PS = 1 << 1,
        CS = 1 << 2
    };
    D3D12_SHADER_VISIBILITY convert(StageVisibility vis){
        using enum StageVisibility;

        switch(vis){
        case VS: return D3D12_SHADER_VISIBILITY_VERTEX;
        case PS: return D3D12_SHADER_VISIBILITY_PIXEL;
        case CS: return D3D12_SHADER_VISIBILITY_ALL;
        default: return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    auto convert(Crowy::RHIAddressMode mode){
        using enum Crowy::RHIAddressMode;

        switch(mode){
        case Wrap  : return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case Clamp : return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:
            std::unreachable();
        }
    }

    auto convert(
        Crowy::RHIFilter min, Crowy::RHIFilter mag, Crowy::RHIFilter mip,
        bool anisotropy, bool comparison
    ){
        using enum Crowy::RHIFilter;

        if(anisotropy)
            return comparison ?
                D3D12_FILTER_COMPARISON_ANISOTROPIC :
                D3D12_FILTER_ANISOTROPIC;

        UINT flags = 0;

        if(mip == Linear) flags |= 0x1;
        if(mag == Linear) flags |= 0x4;
        if(min == Linear) flags |= 0x10;
        if(comparison)               flags |= 0x80;

        return static_cast<D3D12_FILTER>(flags);
    }
}

namespace Crowy
{
    DX12GraphicsPipelineState::DX12GraphicsPipelineState(
        Device& device,
        const RHIGraphicsPipelineStateDesc& desc,
        RootSignature& rootSignature,
        StrView name
    )
    #if defined(_DEBUG) || !defined(NDEBUG)
        : debugName(name)
    #endif
    {
        auto& frontend = std::get<RHILegacyFrontendDesc>(desc.preRasterizer);
        primitiveTopology = ::convert(frontend.topology);

        // Input Layout
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
        if(frontend.vertexLayout.has_value()){
            primitiveTopology = ::convert(frontend.topology);
            const auto& vertexLayout = frontend.vertexLayout.value();

            elements.resize(vertexLayout.size());
            for(usize i=0; i<vertexLayout.size(); ++i){
                const auto& src = vertexLayout[i];
                auto& dst = elements[i];

                dst.SemanticName = src.semanticName;
                dst.SemanticIndex = src.semanticIndex;
                dst.Format = convert(src.format);
                dst.InputSlot = src.inputSlot;
                dst.AlignedByteOffset = src.alignedByteOffset;
                dst.InputSlotClass = src.classification == RHIInputClassification::PerVertex ?
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA :
                    D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                dst.InstanceDataStepRate = src.instanceDataStepRate;
            }
        }

        // Vertex Shader
        RHIShader vertexShader{
            frontend.vertexShader,
            RHIBackend::DirectX12,
            "sm_6_6"
        };

        // RasterizerState
        D3D12_RASTERIZER_DESC rsDesc{
            .FillMode = ::convert(desc.rasterizer.fillMode),
            .CullMode = ::convert(desc.rasterizer.cullMode),
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

        // Pixel Shader
        RHIShader pixelShader(
            desc.fragmentShader,
            RHIBackend::DirectX12,
            "sm_6_6"
        );

        // DepthStencilState
        D3D12_DEPTH_STENCIL_DESC dsDesc{
            .DepthEnable = FALSE,
            .StencilEnable = FALSE
        };
        if(desc.depthStencil.has_value()){
            const auto& depthStencil = desc.depthStencil.value();

            dsDesc.DepthEnable = TRUE;
            dsDesc.DepthWriteMask = depthStencil.depthWriteEnable ?
                D3D12_DEPTH_WRITE_MASK_ALL :
                D3D12_DEPTH_WRITE_MASK_ZERO;
            dsDesc.DepthFunc = convert(depthStencil.depthFunc);

            if(depthStencil.stencil.has_value()){
                const auto& stencil = depthStencil.stencil.value();

                dsDesc.StencilEnable = TRUE;
                dsDesc.StencilReadMask = stencil.readMask;
                dsDesc.StencilWriteMask = stencil.writeMask;
                dsDesc.FrontFace = ::convert(stencil.frontFace);
                dsDesc.BackFace = ::convert(stencil.backFace);
            }
        }

        // BlendState
        const auto blend = desc.blend.value_or(RHIBlendState{});
        D3D12_BLEND_DESC bsDesc{
            .AlphaToCoverageEnable = blend.alphaToCoverageEnable,
            .IndependentBlendEnable = blend.independentBlendEnable
        };

        constexpr auto MAX_RENDER_TARGETS = std::min(
            RHI_MAX_RENDER_TARGETS,
            8u
        );
        for(u32 i=0; i<MAX_RENDER_TARGETS; ++i){
            const auto& src = blend.renderTargets[i];
            auto& dst = bsDesc.RenderTarget[i];

            using enum RHIColorWriteMask;
            static_assert(D3D12_COLOR_WRITE_ENABLE_RED   == static_cast<u8>(EnableRed));
            static_assert(D3D12_COLOR_WRITE_ENABLE_GREEN == static_cast<u8>(EnableGreen));
            static_assert(D3D12_COLOR_WRITE_ENABLE_BLUE  == static_cast<u8>(EnableBlue));
            static_assert(D3D12_COLOR_WRITE_ENABLE_ALPHA == static_cast<u8>(EnableAlpha));

            dst.BlendEnable    = src.blendEnable;
            dst.SrcBlend       = ::convert(src.srcBlend);
            dst.DestBlend      = ::convert(src.dstBlend);
            dst.BlendOp        = ::convert(src.blendOp);
            dst.SrcBlendAlpha  = ::convert(src.srcBlendAlpha);
            dst.DestBlendAlpha = ::convert(src.dstBlendAlpha);
            dst.BlendOpAlpha   = ::convert(src.blendOpAlpha);
            dst.RenderTargetWriteMask = static_cast<UINT8>(src.writeMask);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC dxDesc{
            .pRootSignature = &rootSignature,
            .VS = D3D12_SHADER_BYTECODE{
                .pShaderBytecode = vertexShader.GetBytecode(),
                .BytecodeLength = vertexShader.GetBytecodeLength()
            },
            .PS = D3D12_SHADER_BYTECODE{
                .pShaderBytecode = pixelShader.GetBytecode(),
                .BytecodeLength = pixelShader.GetBytecodeLength()
            },
            .BlendState = bsDesc,
            .SampleMask = UINT_MAX,
            .RasterizerState = rsDesc,
            .DepthStencilState = dsDesc,
            .InputLayout = {
                .pInputElementDescs = elements.data(),
                .NumElements = static_cast<UINT>(elements.size())
            },
            .PrimitiveTopologyType = ::topologyType(primitiveTopology),
            .NumRenderTargets = static_cast<UINT>(desc.renderTargetCount),
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {1, 0},
            .NodeMask = 0,
            .CachedPSO = {nullptr, 0},
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };

        // Render target formats
        for(usize i=0; i<desc.renderTargetCount; ++i){
            dxDesc.RTVFormats[i] = convert(desc.renderTargetFormats[i]);
        }

        CHECK_HRESULT(device.CreateGraphicsPipelineState(
            &dxDesc,
            IID_PPV_ARGS(pipeline.GetAddressOf())
        ), "Failed to create graphics pipeline state");
    }

    DX12GraphicsPipelineState::~DX12GraphicsPipelineState() = default;

    void DX12GraphicsPipelineState::Bind(CommandList& cmdList) const{
        cmdList.IASetPrimitiveTopology(primitiveTopology);
        cmdList.SetPipelineState(pipeline.Get());
    }

    DX12ComputePipelineState::DX12ComputePipelineState(
        Device& device,
        const RHIComputePipelineStateDesc& desc,
        RootSignature& rootSignature,
        StrView name
    )
    #if defined(_DEBUG) || !defined(NDEBUG)
        : debugName(name)
    #endif
    {
        RHIShader computeShader(
            desc.computeShader,
            RHIBackend::DirectX12,
            "sm_6_6"
        );
        if(desc.threadGroupSize.has_value())
            CROWY_ASSERT(
                *desc.threadGroupSize <= computeShader.GetRefl().threadGroupSize,
                "Unexpected thread group size"
            );

        D3D12_COMPUTE_PIPELINE_STATE_DESC dxDesc{
            .pRootSignature = &rootSignature,
            .CS = D3D12_SHADER_BYTECODE{
                .pShaderBytecode = computeShader.GetBytecode(),
                .BytecodeLength = computeShader.GetBytecodeLength()
            },
            .NodeMask = 0,
            .CachedPSO = {nullptr, 0},
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };

        CHECK_HRESULT(device.CreateComputePipelineState(
            &dxDesc,
            IID_PPV_ARGS(pipeline.GetAddressOf())
        ), "Failed to create compute pipeline state");
    }

    DX12ComputePipelineState::~DX12ComputePipelineState() = default;

    void DX12ComputePipelineState::Bind(CommandList& cmdList) const{
        cmdList.SetPipelineState(pipeline.Get());
    }
}
