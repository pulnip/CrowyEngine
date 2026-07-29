#include <utility>
#include <dispatch/dispatch.h>
#include <Metal/MTLComputePipeline.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLLibrary.hpp>
#include <Metal/MTLRenderPipeline.hpp>
#include <Metal/MTLVertexDescriptor.hpp>
#include "Assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "EnumUtil.hpp"
#include "MetalPipelineState.hpp"
#include "MetalUtil.hpp"
#include "RHIShader.hpp"

namespace Crowy
{
    namespace{
        auto convert(RHIPrimitiveTopology topology){
            using enum RHIPrimitiveTopology;

            switch(topology){
            case PointList:     return MTL::PrimitiveTypePoint;
            case LineList:      return MTL::PrimitiveTypeLine;
            case LineStrip:     return MTL::PrimitiveTypeLineStrip;
            case TriangleList:  return MTL::PrimitiveTypeTriangle;
            case TriangleStrip: return MTL::PrimitiveTypeTriangleStrip;
            default:
                std::unreachable();
            }
        }

        inline auto convertVertexFormat(RHIPixelFormat format){
            using enum RHIPixelFormat;

            switch(format){
            case R32_FLOAT:    return MTL::VertexFormatFloat;
            case RG32_FLOAT:   return MTL::VertexFormatFloat2;
            case RGB32_FLOAT:  return MTL::VertexFormatFloat3;
            case RGBA32_FLOAT: return MTL::VertexFormatFloat4;
            case R32_SINT:     return MTL::VertexFormatInt;
            case RG32_SINT:    return MTL::VertexFormatInt2;
            case RGBA32_SINT:  return MTL::VertexFormatInt4;
            case R32_UINT:     return MTL::VertexFormatUInt;
            case RG32_UINT:    return MTL::VertexFormatUInt2;
            case RGBA32_UINT:  return MTL::VertexFormatUInt4;
            case R16_FLOAT:    return MTL::VertexFormatHalf;
            case RG16_FLOAT:   return MTL::VertexFormatHalf2;
            case RGBA16_FLOAT: return MTL::VertexFormatHalf4;
            case RGBA8_UNORM:  return MTL::VertexFormatUChar4Normalized;
            case RGBA8_UINT:   return MTL::VertexFormatUChar4;
            default:
                std::unreachable();
            }
        }

        auto convert(RHIStencilOp op){
            using enum RHIStencilOp;

            switch(op){
            case Keep:      return MTL::StencilOperationKeep;
            case Zero:      return MTL::StencilOperationZero;
            case Replace:   return MTL::StencilOperationReplace;
            case IncrSat:   return MTL::StencilOperationIncrementClamp;
            case DecrSat:   return MTL::StencilOperationDecrementClamp;
            case Invert:    return MTL::StencilOperationInvert;
            case IncrWrap:  return MTL::StencilOperationIncrementWrap;
            case DecrWrap:  return MTL::StencilOperationDecrementWrap;
            default:
                std::unreachable();
            }
        }

        auto convert(RHIBlend blend){
            using enum RHIBlend;

            switch(blend){
            case Zero:           return MTL::BlendFactorZero;
            case One:            return MTL::BlendFactorOne;
            case SrcColor:       return MTL::BlendFactorSourceColor;
            case InvSrcColor:    return MTL::BlendFactorOneMinusSourceColor;
            case SrcAlpha:       return MTL::BlendFactorSourceAlpha;
            case InvSrcAlpha:    return MTL::BlendFactorOneMinusSourceAlpha;
            case DstAlpha:       return MTL::BlendFactorDestinationAlpha;
            case InvDstAlpha:    return MTL::BlendFactorOneMinusDestinationAlpha;
            case DstColor:       return MTL::BlendFactorDestinationColor;
            case InvDstColor:    return MTL::BlendFactorOneMinusDestinationColor;
            case SrcAlphaSat:    return MTL::BlendFactorSourceAlphaSaturated;
            case BlendFactor:    return MTL::BlendFactorBlendColor;
            case InvBlendFactor: return MTL::BlendFactorOneMinusBlendColor;
            default:
                std::unreachable();
            }
        }

        auto convert(RHIBlendOp op){
            using enum RHIBlendOp;

            switch(op){
            case Add:             return MTL::BlendOperationAdd;
            case Subtract:        return MTL::BlendOperationSubtract;
            case ReverseSubtract: return MTL::BlendOperationReverseSubtract;
            case Min:             return MTL::BlendOperationMin;
            case Max:             return MTL::BlendOperationMax;
            default:
                std::unreachable();
            }
        }

        auto convert(RHICullMode mode){
            using enum RHICullMode;

            switch(mode){
            case None:     return MTL::CullModeNone;
            case Front:    return MTL::CullModeFront;
            case Back:     return MTL::CullModeBack;
            default:
                std::unreachable();
            }
        }

        void configureStencil(
            MTL::StencilDescriptor& desc,
            const RHIStencilOpDesc& op
        ){
            desc.setStencilCompareFunction(convert(op.func));
            desc.setStencilFailureOperation(convert(op.stencilFailOp));
            desc.setDepthFailureOperation(convert(op.depthFailOp));
            desc.setDepthStencilPassOperation(convert(op.passOp));
        }

        auto makeLibrary(
            MTL::Device& device,
            RHIShader& shader
        ){
            auto bytecode = shader.GetTargetCode();
            dispatch_data_t data = dispatch_data_create(
                bytecode.data(),
                bytecode.size(),
                nullptr,
                DISPATCH_DATA_DESTRUCTOR_DEFAULT
            );

            NS::Error* error = nullptr;
            MTL::Library* library = device.newLibrary(
                data,
                &error
            );
            dispatch_release(data);

            if(library == nullptr){
                auto msg = error->localizedDescription()->utf8String();
                throw std::runtime_error(msg);
            }

            return library;
        }
    }

    MetalGraphicsPipelineState::MetalGraphicsPipelineState(
        MTL::Device& device,
        const RHIGraphicsPipelineStateDesc& desc,
        StrView name
    )
    #if defined(_DEBUG) || !defined(NDEBUG)
        : debugName(name)
    #endif
    {
        AutoreleasePoolScope _;

        auto& frontend = std::get<RHILegacyFrontendDesc>(desc.preRasterizer);
        auto pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
        topology = convert(frontend.topology);

        // Vertex Layout
        if(frontend.vertexLayout.has_value()){
            const auto& vertexLayout = frontend.vertexLayout.value();
            auto vertexDesc = MTL::VertexDescriptor::alloc()->init();
            NS::UInteger stride = 0;

            for(usize i = 0; i < vertexLayout.size(); ++i){
                const auto& elem = vertexLayout[i];
                auto attr = vertexDesc->attributes()->object(i);

                attr->setFormat(convertVertexFormat(elem.format));
                attr->setOffset(elem.alignedByteOffset);
                attr->setBufferIndex(elem.inputSlot);

                auto elemSize = detail::GetBytesPerPixel(elem.format);
                auto elemEnd = elem.alignedByteOffset + elemSize;
                if(elemEnd > stride) stride = elemEnd;
            }

            auto layout = vertexDesc->layouts()->object(0);
            layout->setStride(stride);
            layout->setStepFunction(MTL::VertexStepFunctionPerVertex);

            pipelineDesc->setVertexDescriptor(vertexDesc);
            vertexDesc->release();
        }

        MTL::Library* library = nullptr;
        // Vertex Shader
        {
            const auto& filePath = frontend.vertexShader.path;
            const auto& entryPoint = frontend.vertexShader.entryPoint;

            RHIShader shaderProgram{
                filePath,
                RHIBackend::Metal
            };
            library = makeLibrary(device, shaderProgram);
        #if defined(_DEBUG) || !defined(NDEBUG)
            library->setLabel(toNSString(toUTF8String(filePath)));
        #endif

            auto func = library->newFunction(toNSString(entryPoint));
        #if defined(_DEBUG) || !defined(NDEBUG)
            auto identifier = std::format("{}_{}", filePath, entryPoint);
            func->setLabel(toNSString(identifier));
        #endif

            pipelineDesc->setVertexFunction(func);
        }

        // Store rasterizer state for command list
        rasterizerState = desc.rasterizer;

        // Fragment Shader
        if(desc.fragmentShader.path != frontend.vertexShader.path){
            library->release();

            const auto& filePath = desc.fragmentShader.path;

            RHIShader shaderProgram{
                filePath,
                RHIBackend::Metal
            };
            library = makeLibrary(device, shaderProgram);
        #if defined(_DEBUG) || !defined(NDEBUG)
            library->setLabel(toNSString(toUTF8String(filePath)));
        #endif
        }

        {
            const auto& filePath = desc.fragmentShader.path;
            const auto& entryPoint = desc.fragmentShader.entryPoint;

            auto func = library->newFunction(toNSString(entryPoint));
        #if defined(_DEBUG) || !defined(NDEBUG)
            auto identifier = std::format("{}_{}", filePath, entryPoint);
            func->setLabel(toNSString(identifier));
        #endif

            pipelineDesc->setFragmentFunction(func);
        }
        // func holds reference
        library->release();

        if(desc.depthStencil.has_value()){
            // Depth Stencil State
            createDepthStencilState(device, desc.depthStencil.value());

            // Depth Stencil Format
            auto depthStencilFormat = desc.depthStencil->format;
            CROWY_ASSERT(depthStencilFormat != RHIPixelFormat::Unknown);

            pipelineDesc->setDepthAttachmentPixelFormat(
                convert(depthStencilFormat)
            );
        }

        // Blend State
        const auto blend = desc.blend.value_or(RHIBlendState{});
        pipelineDesc->setAlphaToCoverageEnabled(
            blend.alphaToCoverageEnable
        );

        for(usize i = 0; i < desc.renderTargetCount; ++i){
            const auto& rtBlend = blend.renderTargets[
                blend.independentBlendEnable ? i : 0
            ];
            auto colorAttach = pipelineDesc->colorAttachments()->object(i);

            const auto blendEnabled = !blend.independentBlendEnable || rtBlend.blendEnable;
            colorAttach->setBlendingEnabled(blendEnabled);

            if(blendEnabled){
                colorAttach->setSourceRGBBlendFactor(
                    convert(rtBlend.srcBlend)
                );
                colorAttach->setDestinationRGBBlendFactor(
                    convert(rtBlend.dstBlend)
                );
                colorAttach->setRgbBlendOperation(
                    convert(rtBlend.blendOp)
                );
                colorAttach->setSourceAlphaBlendFactor(
                    convert(rtBlend.srcBlendAlpha)
                );
                colorAttach->setDestinationAlphaBlendFactor(
                    convert(rtBlend.dstBlendAlpha)
                );
                colorAttach->setAlphaBlendOperation(
                    convert(rtBlend.blendOpAlpha)
                );

                MTL::ColorWriteMask writeMask = MTL::ColorWriteMaskNone;

                using enum RHIColorWriteMask;
                if(hasFlag(rtBlend.writeMask, EnableRed))
                    writeMask |= MTL::ColorWriteMaskRed;
                if(hasFlag(rtBlend.writeMask, EnableGreen))
                    writeMask |= MTL::ColorWriteMaskGreen;
                if(hasFlag(rtBlend.writeMask, EnableBlue))
                    writeMask |= MTL::ColorWriteMaskBlue;
                if(hasFlag(rtBlend.writeMask, EnableAlpha))
                    writeMask |= MTL::ColorWriteMaskAlpha;

                colorAttach->setWriteMask(writeMask);
            }
        }

        // Render Target Formats
        for(usize i = 0; i < desc.renderTargetCount; ++i){
            auto colorAttach = pipelineDesc->colorAttachments()->object(i);

            colorAttach->setPixelFormat(
                convert(desc.renderTargetFormats[i])
            );
        }

        NS::Error* error = nullptr;
        pipeline = NS::TransferPtr(device.newRenderPipelineState(
            pipelineDesc,
            &error
        ));
        pipelineDesc->release();

        if(!pipeline){
            auto msg = error->localizedDescription()->utf8String();
            throw std::runtime_error(msg);
        }
    }

    MetalGraphicsPipelineState::~MetalGraphicsPipelineState() = default;

    void MetalGraphicsPipelineState::Bind(MTL::RenderCommandEncoder& encoder){
        encoder.setRenderPipelineState(pipeline.get());

        // Rasterizer state
        encoder.setCullMode(convert(rasterizerState.cullMode));
        encoder.setFrontFacingWinding(
            rasterizerState.frontCounterClockwise ?
                MTL::WindingCounterClockwise :
                MTL::WindingClockwise
        );
        encoder.setTriangleFillMode(
            rasterizerState.fillMode == RHIFillMode::Wireframe ?
                MTL::TriangleFillModeLines :
                MTL::TriangleFillModeFill
        );
        encoder.setDepthBias(
            rasterizerState.depthBias,
            rasterizerState.slopeScaledDepthBias,
            rasterizerState.depthBiasClamp
        );
        encoder.setDepthClipMode(
            rasterizerState.depthClipEnable ?
                MTL::DepthClipModeClip :
                MTL::DepthClipModeClamp
        );

        encoder.setDepthStencilState(depthStencilState.get());
    }

    void MetalGraphicsPipelineState::createDepthStencilState(
        MTL::Device& device,
        const RHIDepthStencilState& desc
    ){
        auto dsDesc = MTL::DepthStencilDescriptor::alloc()->init();

        dsDesc->setDepthWriteEnabled(desc.depthWriteEnable);
        dsDesc->setDepthCompareFunction(
            convert(desc.depthFunc)
        );

        if(desc.stencil.has_value()){
            auto mtlDesc = MTL::StencilDescriptor::alloc()->init();
            mtlDesc->setReadMask(desc.stencil->readMask);
            mtlDesc->setWriteMask(desc.stencil->writeMask);

            configureStencil(*mtlDesc, desc.stencil->frontFace);
            dsDesc->setFrontFaceStencil(mtlDesc);

            configureStencil(*mtlDesc, desc.stencil->backFace);
            dsDesc->setBackFaceStencil(mtlDesc);

            mtlDesc->release();
        }

        depthStencilState = NS::TransferPtr(device.newDepthStencilState(dsDesc));
        dsDesc->release();
    }

    MetalComputePipelineState::MetalComputePipelineState(
        MTL::Device& device,
        const RHIComputePipelineStateDesc& desc,
        StrView name
    )
    #if defined(_DEBUG) || !defined(NDEBUG)
        : debugName(name)
    #endif
    {
        AutoreleasePoolScope _;

        const auto& filePath = desc.computeShader.path;
        const auto& entryPoint = desc.computeShader.entryPoint;

        RHIShader shaderProgram{
            filePath,
            RHIBackend::Metal
        };
        auto library = makeLibrary(device, shaderProgram);
    #if defined(_DEBUG) || !defined(NDEBUG)
        library->setLabel(toNSString(toUTF8String(filePath)));
    #endif

        auto func = library->newFunction(toNSString(entryPoint));
    #if defined(_DEBUG) || !defined(NDEBUG)
        auto identifier = std::format("{}_{}", filePath, entryPoint);
        func->setLabel(toNSString(identifier));
    #endif
        // func holds reference
        library->release();

        if(func == nullptr){
            throw std::runtime_error("Compute shader is null");
        }

        auto pipelineDesc = MTL::ComputePipelineDescriptor::alloc()->init();
        pipelineDesc->setComputeFunction(func);

        MTL::AutoreleasedComputePipelineReflection refl = nullptr;
        NS::Error* error = nullptr;
        pipeline = NS::TransferPtr(device.newComputePipelineState(
            pipelineDesc,
            MTL::PipelineOptionBindingInfo,
            &refl,
            &error
        ));
        pipelineDesc->release();

        if(!pipeline){
            throw std::runtime_error("Failed to create compute pipeline state");
        }

        const auto threadGroupSize = shaderProgram.GetThreadGroupSize(entryPoint);
        threadsPerThreadgroup = MTL::Size::Make(
            threadGroupSize.x,
            threadGroupSize.y,
            threadGroupSize.z
        );
    }

    MetalComputePipelineState::~MetalComputePipelineState() = default;

    void MetalComputePipelineState::Bind(MTL::ComputeCommandEncoder& encoder){
        encoder.setComputePipelineState(pipeline.get());
    }
}
