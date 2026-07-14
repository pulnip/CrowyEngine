
#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLRenderPipeline.hpp>
#include <filesystem>
#include <print>
#include <utility>
#include <Metal/MTLLibrary.hpp>
#include <Foundation/NSTypes.hpp>
#include <Metal/MTLArgument.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLComputePipeline.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/Metal.hpp>
#include "Assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "EnumUtil.hpp"
#include "MetalUtil.hpp"
#include "RHIDefinitions.hpp"
#include "StringUtil.hpp"
#include "MetalPipelineState.hpp"

namespace{
    auto convert(Crowy::RHIPrimitiveTopology topology){
        using namespace Crowy;
        using enum RHIPrimitiveTopology;
        using namespace MTL;

        switch(topology){
        case PointList:     return PrimitiveTypePoint;
        case LineList:      return PrimitiveTypeLine;
        case LineStrip:     return PrimitiveTypeLineStrip;
        case TriangleList:  return PrimitiveTypeTriangle;
        case TriangleStrip: return PrimitiveTypeTriangleStrip;
        default:
            std::unreachable();
        }
    }

    inline auto convertVertexFormat(Crowy::RHIPixelFormat format){
        using namespace Crowy;
        using enum RHIPixelFormat;
        using namespace MTL;

        switch(format){
        case R32_FLOAT:    return VertexFormatFloat;
        case RG32_FLOAT:   return VertexFormatFloat2;
        case RGB32_FLOAT:  return VertexFormatFloat3;
        case RGBA32_FLOAT: return VertexFormatFloat4;
        case R32_SINT:     return VertexFormatInt;
        case RG32_SINT:    return VertexFormatInt2;
        case RGBA32_SINT:  return VertexFormatInt4;
        case R32_UINT:     return VertexFormatUInt;
        case RG32_UINT:    return VertexFormatUInt2;
        case RGBA32_UINT:  return VertexFormatUInt4;
        case R16_FLOAT:    return VertexFormatHalf;
        case RG16_FLOAT:   return VertexFormatHalf2;
        case RGBA16_FLOAT: return VertexFormatHalf4;
        case RGBA8_UNORM:  return VertexFormatUChar4Normalized;
        case RGBA8_UINT:   return VertexFormatUChar4;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIStencilOp op){
        using namespace Crowy;
        using enum RHIStencilOp;
        using namespace MTL;

        switch(op){
        case Keep:      return StencilOperationKeep;
        case Zero:      return StencilOperationZero;
        case Replace:   return StencilOperationReplace;
        case IncrSat:   return StencilOperationIncrementClamp;
        case DecrSat:   return StencilOperationDecrementClamp;
        case Invert:    return StencilOperationInvert;
        case IncrWrap:  return StencilOperationIncrementWrap;
        case DecrWrap:  return StencilOperationDecrementWrap;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIBlend blend){
        using namespace Crowy;
        using enum RHIBlend;
        using namespace MTL;

        switch(blend){
        case Zero:           return BlendFactorZero;
        case One:            return BlendFactorOne;
        case SrcColor:       return BlendFactorSourceColor;
        case InvSrcColor:    return BlendFactorOneMinusSourceColor;
        case SrcAlpha:       return BlendFactorSourceAlpha;
        case InvSrcAlpha:    return BlendFactorOneMinusSourceAlpha;
        case DstAlpha:       return BlendFactorDestinationAlpha;
        case InvDstAlpha:    return BlendFactorOneMinusDestinationAlpha;
        case DstColor:       return BlendFactorDestinationColor;
        case InvDstColor:    return BlendFactorOneMinusDestinationColor;
        case SrcAlphaSat:    return BlendFactorSourceAlphaSaturated;
        case BlendFactor:    return BlendFactorBlendColor;
        case InvBlendFactor: return BlendFactorOneMinusBlendColor;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIBlendOp op){
        using namespace Crowy;
        using enum RHIBlendOp;
        using namespace MTL;

        switch(op){
        case Add:             return BlendOperationAdd;
        case Subtract:        return BlendOperationSubtract;
        case ReverseSubtract: return BlendOperationReverseSubtract;
        case Min:             return BlendOperationMin;
        case Max:             return BlendOperationMax;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHICullMode mode){
        using namespace Crowy;
        using enum RHICullMode;
        using namespace MTL;

        switch(mode){
        case None:     return CullModeNone;
        case Front:    return CullModeFront;
        case Back:     return CullModeBack;
        default:
            std::unreachable();
        }
    }

    void configureStencil(
        MTL::StencilDescriptor& desc,
        const Crowy::RHIStencilOpDesc& op
    ){
        desc.setStencilCompareFunction(convert(op.func));
        desc.setStencilFailureOperation(convert(op.stencilFailOp));
        desc.setDepthFailureOperation(convert(op.depthFailOp));
        desc.setDepthStencilPassOperation(convert(op.passOp));
    }

    auto convert(MTL::BindingAccess access){
        using namespace Crowy;
        using enum RHIBindingAccess;
        using namespace MTL;

        switch(access){
        case BindingAccessReadOnly:  return ReadOnly;
        case BindingAccessReadWrite: return ReadWrite;
        case BindingAccessWriteOnly: return WriteOnly;
        default:
            std::unreachable();
        }
    }

    MTL::Function* compileShader(
        MTL::Device& device,
        const Crowy::RHIShaderDesc& desc
    ){
        auto& filePath = desc.path;
        auto& entryPoint = desc.entryPoint;

        using namespace Crowy;

        NS::Error* error = nullptr;
        MTL::Library* library;

        auto ext = std::filesystem::path(filePath).extension().string();

        if(ext == ".metal"){
            auto code = readFileAsString(filePath);

            auto source = NS::String::string(code.c_str(), NS::UTF8StringEncoding);
            library = device.newLibrary(source, nullptr, &error);
        }
        else if(ext == ".metallib"){
            auto path = NS::String::string(filePath.c_str(), NS::UTF8StringEncoding);
            auto url = NS::URL::fileURLWithPath(path);
            library = device.newLibrary(url, &error);
        }
        else{
            throw std::runtime_error("Unknown file format: " + ext);
        }

        if(library == nullptr){
            Str errorMsg = error->localizedDescription()->utf8String();
            throw std::runtime_error(
                "Shader compile failed: " + errorMsg
            );
        }

        auto entry = NS::String::string(entryPoint.data(), NS::UTF8StringEncoding);
        auto func = library->newFunction(entry);
        // func holds reference
        library->release();

        if(func == nullptr){
            throw std::runtime_error(
                std::format("Entry point not found: {}", entryPoint)
            );
        }

    #if defined(_DEBUG) || !defined(NDEBUG)
        auto identifier = std::format("{}_{}", filePath.c_str(), entryPoint);
        func->setLabel(
            NS::String::string(identifier.c_str(), NS::UTF8StringEncoding)
        );
    #endif

        return func;
    }

    auto extractBindingInfo(NS::Array& bindings){
        using namespace Crowy;

        RHIShaderBindingInfo info;

        for(NS::UInteger i=0; i<bindings.count(); ++i){
            auto obj = bindings.object(i);
            auto binding = static_cast<MTL::Binding*>(obj);

            // shader parameter name
            auto name = binding->name();
            // shader parameter slot number, ex. [[buffer(0)]]
            auto index = binding->index();
            // Buffer / Texture / Sampler / ...
            auto type = binding->type();
            // ReadOnly / WriteOnly / ReadWrite
            auto access = binding->access();
            // check for optimized or not
            auto used = binding->used();

            RHISlotBindingInfo slotInfo{
                .index = static_cast<u32>(index),
                .access = convert(access)
            };

            if(!used)
                // TODO. use integrated logging system later.
                std::println("[Warn] {} is not used.", name->utf8String());

            switch(type){
            case MTL::BindingTypeBuffer: {
                // auto b = static_cast<MTL::BufferBinding*>(binding);
                info.bufferInfo.emplace(name->utf8String(), std::move(slotInfo));
            } break;
            case MTL::BindingTypeTexture: {
                // auto b = static_cast<MTL::TextureBinding*>(binding);
                info.textureInfo.emplace(name->utf8String(), std::move(slotInfo));
            } break;
            case MTL::BindingTypeSampler: {
                info.samplerInfo.emplace(name->utf8String(), std::move(slotInfo));
            }
            default:
            }
        }

        return info;
    }

    inline auto extractBindingInfo(MTL::RenderPipelineReflection& refl){
        return Crowy::RHIGraphicsBindingInfo{
            .vsInfo = extractBindingInfo(*refl.vertexBindings()),
            .fsInfo = extractBindingInfo(*refl.fragmentBindings())
        };
    }

    inline auto extractBindingInfo(MTL::ComputePipelineReflection& refl){
        return Crowy::RHIComputeBindingInfo{
            .csInfo = extractBindingInfo(*refl.bindings())
        };
    }
}

namespace Crowy
{
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

        topology = ::convert(frontend.topology);

        pipelineDesc->setVertexFunction(
            compileShader(device, frontend.vertexShader)
        );

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

                auto elemSize = getBytesPerPixel(elem.format);
                auto elemEnd = elem.alignedByteOffset + elemSize;
                if(elemEnd > stride) stride = elemEnd;
            }

            auto layout = vertexDesc->layouts()->object(0);
            layout->setStride(stride);
            layout->setStepFunction(MTL::VertexStepFunctionPerVertex);

            pipelineDesc->setVertexDescriptor(vertexDesc);
            vertexDesc->release();
        }

        pipelineDesc->setFragmentFunction(
            compileShader(device, desc.fragmentShader)
        );

        // Render Target Formats & Blend States
        for(usize i = 0; i < desc.renderTargetCount; ++i){
            auto colorAttach = pipelineDesc->colorAttachments()->object(i);
            colorAttach->setPixelFormat(
                convert(desc.renderTargetFormats[i])
            );
            MTL::ColorWriteMask writeMask = MTL::ColorWriteMaskAll;

            if(desc.blend.has_value()){
                const auto& blend = desc.blend.value();
                const auto& rtBlend = blend.renderTargets[i];
                colorAttach->setBlendingEnabled(rtBlend.blendEnable);

                if(rtBlend.blendEnable){
                    colorAttach->setSourceRGBBlendFactor(
                        ::convert(rtBlend.srcBlend)
                    );
                    colorAttach->setDestinationRGBBlendFactor(
                        ::convert(rtBlend.dstBlend)
                    );
                    colorAttach->setRgbBlendOperation(
                        ::convert(rtBlend.blendOp)
                    );
                    colorAttach->setSourceAlphaBlendFactor(
                        ::convert(rtBlend.srcBlendAlpha)
                    );
                    colorAttach->setDestinationAlphaBlendFactor(
                        ::convert(rtBlend.dstBlendAlpha)
                    );
                    colorAttach->setAlphaBlendOperation(
                        ::convert(rtBlend.blendOpAlpha)
                    );
                }

                writeMask = MTL::ColorWriteMaskNone;

                using enum RHIColorWriteMask;

                if(hasFlag(rtBlend.writeMask, EnableRed))
                    writeMask |= MTL::ColorWriteMaskRed;
                if(hasFlag(rtBlend.writeMask, EnableGreen))
                    writeMask |= MTL::ColorWriteMaskGreen;
                if(hasFlag(rtBlend.writeMask, EnableBlue))
                    writeMask |= MTL::ColorWriteMaskBlue;
                if(hasFlag(rtBlend.writeMask, EnableAlpha))
                    writeMask |= MTL::ColorWriteMaskAlpha;
            }

            colorAttach->setWriteMask(writeMask);
        }

        // Depth Stencil Format
        if(desc.depthStencil.has_value()){
            auto depthStencilFormat = desc.depthStencil->format;
            CROWY_ASSERT(depthStencilFormat != RHIPixelFormat::Unknown);

            pipelineDesc->setDepthAttachmentPixelFormat(
                convert(depthStencilFormat)
            );
        }

        // Alpha to Coverage
        if(desc.blend.has_value()){
            auto& blend = desc.blend.value();

            pipelineDesc->setAlphaToCoverageEnabled(
                blend.alphaToCoverageEnable
            );
        }

        MTL::AutoreleasedRenderPipelineReflection refl = nullptr;
        NS::Error* error = nullptr;
        pipeline = device.newRenderPipelineState(
            pipelineDesc,
            MTL::PipelineOptionBindingInfo,
            &refl,
            &error
        );
        pipelineDesc->release();

        bindingInfo = extractBindingInfo(*refl);

        if(pipeline == nullptr){
            auto msg = error->localizedDescription()->utf8String();
            throw std::runtime_error(msg);
        }

        // Depth Stencil State
        if(desc.depthStencil.has_value()){
            createDepthStencilState(device, desc.depthStencil.value());
        }

        // Store rasterizer state for command list
        rasterizerState = desc.rasterizer;
    }

    MetalGraphicsPipelineState::~MetalGraphicsPipelineState(){
        if(pipeline != nullptr){
            pipeline->release();
            pipeline = nullptr;
        }
        if(depthStencilState != nullptr){
            depthStencilState->release();
            depthStencilState = nullptr;
        }
    }

    void MetalGraphicsPipelineState::Bind(MTL::RenderCommandEncoder& encoder){
        encoder.setRenderPipelineState(pipeline);

        if(depthStencilState != nullptr){
            encoder.setDepthStencilState(depthStencilState);
        }

        // Rasterizer state
        encoder.setCullMode(::convert(rasterizerState.cullMode));
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

        depthStencilState = device.newDepthStencilState(dsDesc);
        dsDesc->release();
    }

    MetalComputePipelineState::MetalComputePipelineState(
        MTL::Device& device,
        const RHIComputePipelineStateDesc& desc,
        StrView name
    )
        : cs(compileShader(device, desc.computeShader))
        , debugName(name)
    {
        AutoreleasePoolScope _;

        if(cs == nullptr){
            throw std::runtime_error("Compute shader is null");
        }

        auto pipelineDesc = MTL::ComputePipelineDescriptor::alloc()->init();
        pipelineDesc->setComputeFunction(cs);

        MTL::AutoreleasedComputePipelineReflection refl = nullptr;
        NS::Error* error = nullptr;
        pipeline = device.newComputePipelineState(
            pipelineDesc,
            MTL::PipelineOptionBindingInfo,
            &refl,
            &error
        );
        pipelineDesc->release();

        bindingInfo = extractBindingInfo(*refl);

        if(pipeline == nullptr){
            throw std::runtime_error("Failed to create compute pipeline state");
        }

        if(desc.threadGroupSize.has_value()){
            const auto& threadGroupSize = *desc.threadGroupSize;
            threadsPerThreadgroup = MTL::Size::Make(
                threadGroupSize.x,
                threadGroupSize.y,
                threadGroupSize.z
            );
        }
        else{
            auto effectiveGroupSize = std::min(
                256ul,
                pipeline->maxTotalThreadsPerThreadgroup()
            );
            threadsPerThreadgroup = DefaultGroupSize(
                effectiveGroupSize,
                desc.gridSize
            );
        }
    }

    MetalComputePipelineState::~MetalComputePipelineState(){
        if(pipeline != nullptr){
            pipeline->release();
            pipeline = nullptr;
        }
        if(cs != nullptr){
            cs->release();
            cs = nullptr;
        }
    }

    void MetalComputePipelineState::Bind(MTL::ComputeCommandEncoder& encoder){
        encoder.setComputePipelineState(pipeline);
    }

    MTL::Size MetalComputePipelineState::DefaultGroupSize(
        u32 numThreads,
        const Size3D& gridSize
    ) noexcept{
        auto width = std::min(numThreads, gridSize.x);
        numThreads /= width;
        auto height = std::min(numThreads, gridSize.y);
        numThreads /= height;
        auto depth = std::min(numThreads, gridSize.z);

        return MTL::Size::Make(width, height, depth);
    }
}
