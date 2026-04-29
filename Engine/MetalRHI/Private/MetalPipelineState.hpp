#pragma once

#include <Metal/MTLLibrary.hpp>
#include <filesystem>
#include <utility>
#include <Foundation/NSTypes.hpp>
#include <Metal/MTLArgument.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLComputePipeline.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/Metal.hpp>
#include "assert.hpp"
#include "string.hpp"
#include "AutoreleasePoolScope.hpp"
#include "MetalUtil.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
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

    inline auto convert(RHIStencilOp op){
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

    inline auto convert(RHIBlend blend){
        using enum RHIBlend;

        switch(blend){
        case Zero:           return MTL::BlendFactorZero;
        case One:            return MTL::BlendFactorOne;
        case SrcColor:       return MTL::BlendFactorSourceColor;
        case InvSrcColor:    return MTL::BlendFactorOneMinusSourceColor;
        case SrcAlpha:       return MTL::BlendFactorSourceAlpha;
        case InvSrcAlpha:    return MTL::BlendFactorOneMinusSourceAlpha;
        case DestAlpha:      return MTL::BlendFactorDestinationAlpha;
        case InvDestAlpha:   return MTL::BlendFactorOneMinusDestinationAlpha;
        case DestColor:      return MTL::BlendFactorDestinationColor;
        case InvDestColor:   return MTL::BlendFactorOneMinusDestinationColor;
        case SrcAlphaSat:    return MTL::BlendFactorSourceAlphaSaturated;
        case BlendFactor:    return MTL::BlendFactorBlendColor;
        case InvBlendFactor: return MTL::BlendFactorOneMinusBlendColor;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHIBlendOp op){
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

    inline void configureStencil(
        MTL::StencilDescriptor& desc,
        const RHIStencilOpDesc& op
    ){
        desc.setStencilCompareFunction(convert(op.func));
        desc.setStencilFailureOperation(convert(op.stencilFailOp));
        desc.setDepthFailureOperation(convert(op.depthFailOp));
        desc.setDepthStencilPassOperation(convert(op.passOp));
    }

    inline auto convert(MTL::BindingAccess access){
        using enum RHIBindingAccess;

        switch(access){
        case MTL::BindingAccessReadOnly:  return ReadOnly;
        case MTL::BindingAccessReadWrite: return ReadWrite;
        case MTL::BindingAccessWriteOnly: return WriteOnly;
        default:
            std::unreachable();
        }
    }

    inline MTL::Function* compileShader(
        MTL::Device& device,
        const std::filesystem::path& shaderFilePath,
        const std::string& shaderEntryPoint
    ){
        NS::Error* error = nullptr;
        MTL::Library* library;

        auto ext = std::filesystem::path(shaderFilePath).extension().string();

        if(ext == ".metal"){
            auto code = read_file_as_string(shaderFilePath);

            auto source = NS::String::string(code.c_str(), NS::UTF8StringEncoding);
            library = device.newLibrary(source, nullptr, &error);
        }
        else if(ext == ".metallib"){
            auto path = NS::String::string(shaderFilePath.c_str(), NS::UTF8StringEncoding);
            auto url = NS::URL::fileURLWithPath(path);
            library = device.newLibrary(url, &error);
        }
        else{
            throw std::runtime_error("Unknown file format: " + ext);
        }

        if(library == nullptr){
            std::string errorMsg = error->localizedDescription()->utf8String();
            throw std::runtime_error(
                "Shader compile failed: " + errorMsg
            );
        }

        auto entry = NS::String::string(shaderEntryPoint.c_str(), NS::UTF8StringEncoding);
        auto func = library->newFunction(entry);
        // func holds reference
        library->release();

        if(!func){
            throw std::runtime_error(
                "Entry point not found:" + shaderEntryPoint
            );
        }

    #if defined(_DEBUG) || !defined(NDEBUG)
        auto identifier = std::format("{}_{}", shaderFilePath.c_str(), shaderEntryPoint);
        func->setLabel(
            NS::String::string(identifier.c_str(), NS::UTF8StringEncoding)
        );
    #endif

        return func;
    }

    inline auto extractBindingInfo(NS::Array& bindings){
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
                .index = static_cast<uint32_t>(index),
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
        return RHIGraphicsBindingInfo{
            .vsInfo = extractBindingInfo(*refl.vertexBindings()),
            .fsInfo = extractBindingInfo(*refl.fragmentBindings())
        };
    }

    inline auto extractBindingInfo(MTL::ComputePipelineReflection& refl){
        return RHIComputeBindingInfo{
            .csInfo = extractBindingInfo(*refl.bindings())
        };
    }

    class MetalGraphicsPipelineState
#ifndef USE_STATIC_RHI
        : public RHIGraphicsPipelineState
#endif
    {
    private:
        MTL::Function* vs = nullptr;
        MTL::Function* fs = nullptr;
        MTL::RenderPipelineState* renderPipeline = nullptr;
        MTL::DepthStencilState* depthStencilState = nullptr;
        RHIRasterizerState rasterizerState{};
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;

        RHIGraphicsBindingInfo bindingInfo;

        const std::string debugName;

    public:
        MetalGraphicsPipelineState(
            MTL::Device& device,
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        )
            : vs(compileShader(device, desc.vertexShaderPath, desc.vertexShaderEntryPoint))
            , fs(compileShader(device, desc.fragmentShaderPath, desc.fragmentShaderEntryPoint))
            , debugName(name)
        {
            AutoreleasePoolScope _;

            if(vs == nullptr || fs == nullptr){
                throw std::runtime_error("Vertex shader or Fragment shader is null");
            }

            auto pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
            pipelineDesc->setVertexFunction(vs);
            pipelineDesc->setFragmentFunction(fs);

            // Vertex Layout
            if(desc.vertexLayout.has_value()){
                const auto& vertexLayout = desc.vertexLayout.value();
                auto vertexDesc = MTL::VertexDescriptor::alloc()->init();
                size_t stride = 0;

                for(uint32_t i = 0; i < vertexLayout.size(); ++i){
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

            // Render Target Formats & Blend States
            for(uint32_t i = 0; i < desc.renderTargetCount; ++i){
                auto colorAttach = pipelineDesc->colorAttachments()->object(i);
                colorAttach->setPixelFormat(
                    convertPixelFormat(desc.renderTargetFormats[i])
                );
                MTL::ColorWriteMask writeMask = MTL::ColorWriteMaskNone;

                if(desc.blend.has_value()){
                    const auto& blend = desc.blend.value();
                    const auto& rtBlend = blend.renderTargets[i];
                    colorAttach->setBlendingEnabled(rtBlend.blendEnable);
    
                    if(rtBlend.blendEnable){
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
                    }

                    if(rtBlend.renderTargetWriteMask & 0x1) writeMask |= MTL::ColorWriteMaskRed;
                    if(rtBlend.renderTargetWriteMask & 0x2) writeMask |= MTL::ColorWriteMaskGreen;
                    if(rtBlend.renderTargetWriteMask & 0x4) writeMask |= MTL::ColorWriteMaskBlue;
                    if(rtBlend.renderTargetWriteMask & 0x8) writeMask |= MTL::ColorWriteMaskAlpha;
                }

                colorAttach->setWriteMask(writeMask);
            }

            // Depth Stencil Format
            if(desc.depthStencil.has_value()){
                auto depthStencilFormat = desc.depthStencil->format;
                CROWY_ASSERT(depthStencilFormat != RHIPixelFormat::Unknown);

                pipelineDesc->setDepthAttachmentPixelFormat(
                    convertPixelFormat(depthStencilFormat)
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
            renderPipeline = device.newRenderPipelineState(
                pipelineDesc,
                MTL::PipelineOptionBindingInfo,
                &refl,
                &error
            );
            pipelineDesc->release();

            bindingInfo = extractBindingInfo(*refl);

            if(renderPipeline == nullptr){
                const char* msg = error->localizedDescription()->utf8String();
                throw std::runtime_error(msg);
            }

            // Depth Stencil State
            if(desc.depthStencil.has_value()){
                createDepthStencilState(device, desc.depthStencil.value());
            }

            // Store rasterizer state for command list
            rasterizerState = desc.rasterizer;
            topology = desc.topology;
        }

        ~MetalGraphicsPipelineState(){
            if(renderPipeline != nullptr){
                renderPipeline->release();
                renderPipeline = nullptr;
            }
            if(depthStencilState != nullptr){
                depthStencilState->release();
                depthStencilState = nullptr;
            }
            if(fs != nullptr){
                fs->release();
                fs = nullptr;
            }
            if(vs != nullptr){
                vs->release();
                vs = nullptr;
            }
        }

        const RHIGraphicsBindingInfo& getInfo() const RHI_OVERRIDE{
            return bindingInfo;
        }

        MTL::RenderPipelineState* get() const{ 
            return renderPipeline; 
        }

        MTL::DepthStencilState* getDepthStencilState() const{ 
            return depthStencilState; 
        }

        const RHIRasterizerState& getRasterizerState() const{ 
            return rasterizerState; 
        }

        RHIPrimitiveTopology getTopology() const{
            return topology;
        }

    private:
        void createDepthStencilState(
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
    };

    class MetalComputePipelineState
#ifndef USE_STATIC_RHI
        : public RHIComputePipelineState
#endif
    {
    private:
        MTL::Function* cs = nullptr;
        MTL::ComputePipelineState* computePipeline = nullptr;
        MTL::Size threadsPerThreadgroup = {0, 0, 0};

        RHIComputeBindingInfo bindingInfo;

        const std::string debugName;

    public:
        MetalComputePipelineState(
            MTL::Device& device,
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        )
            : cs(compileShader(device, desc.computeShaderPath, desc.computeShaderEntryPoint))
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
            computePipeline = device.newComputePipelineState(
                pipelineDesc,
                MTL::PipelineOptionBindingInfo,
                &refl,
                &error
            );
            pipelineDesc->release();

            bindingInfo = extractBindingInfo(*refl);

            if(computePipeline == nullptr){
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
                    computePipeline->maxTotalThreadsPerThreadgroup()
                );
                threadsPerThreadgroup = defaultGroupSize(
                    effectiveGroupSize,
                    desc.gridSize
                );
            }
        }

        ~MetalComputePipelineState(){
            if(computePipeline != nullptr){
                computePipeline->release();
                computePipeline = nullptr;
            }
            if(cs != nullptr){
                cs->release();
                cs = nullptr;
            }
        }

        const RHIComputeBindingInfo& getInfo() const RHI_OVERRIDE{
            return bindingInfo;
        }

        MTL::ComputePipelineState* get() const{ 
            return computePipeline; 
        }

        MTL::Size getThreadsPerThreadgroup() const{
            return threadsPerThreadgroup;
        }

    private:
        static MTL::Size defaultGroupSize(
            uint32_t numThreads,
            const RHISize3D& gridSize
        ) noexcept{
            auto width = std::min(numThreads, gridSize.x);
            numThreads /= width;
            auto height = std::min(numThreads, gridSize.y);
            numThreads /= height;
            auto depth = std::min(numThreads, gridSize.z);

            return MTL::Size::Make(width, height, depth);
        }
    };
}