#pragma once

#include <Foundation/NSTypes.hpp>
#include <Metal/MTLArgument.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLComputePipeline.hpp>
#include <Metal/MTLDevice.hpp>
#include <utility>
#include <Metal/Metal.hpp>
#include "assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "MetalShader.hpp"
#include "MetalUtil.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
    inline auto convertVertexFormat(RHITextureFormat format){
        switch(format){
        case RHITextureFormat::R32_FLOAT:    return MTL::VertexFormatFloat;
        case RHITextureFormat::RG32_FLOAT:   return MTL::VertexFormatFloat2;
        case RHITextureFormat::RGB32_FLOAT:  return MTL::VertexFormatFloat3;
        case RHITextureFormat::RGBA32_FLOAT: return MTL::VertexFormatFloat4;
        case RHITextureFormat::R32_SINT:     return MTL::VertexFormatInt;
        case RHITextureFormat::RG32_SINT:    return MTL::VertexFormatInt2;
        case RHITextureFormat::RGBA32_SINT:  return MTL::VertexFormatInt4;
        case RHITextureFormat::R32_UINT:     return MTL::VertexFormatUInt;
        case RHITextureFormat::RG32_UINT:    return MTL::VertexFormatUInt2;
        case RHITextureFormat::RGBA32_UINT:  return MTL::VertexFormatUInt4;
        case RHITextureFormat::R16_FLOAT:    return MTL::VertexFormatHalf;
        case RHITextureFormat::RG16_FLOAT:   return MTL::VertexFormatHalf2;
        case RHITextureFormat::RGBA16_FLOAT: return MTL::VertexFormatHalf4;
        case RHITextureFormat::RGBA8_UNORM:  return MTL::VertexFormatUChar4Normalized;
        case RHITextureFormat::RGBA8_UINT:   return MTL::VertexFormatUChar4;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHIStencilOp op){
        switch(op){
        case RHIStencilOp::Keep:      return MTL::StencilOperationKeep;
        case RHIStencilOp::Zero:      return MTL::StencilOperationZero;
        case RHIStencilOp::Replace:   return MTL::StencilOperationReplace;
        case RHIStencilOp::IncrSat:   return MTL::StencilOperationIncrementClamp;
        case RHIStencilOp::DecrSat:   return MTL::StencilOperationDecrementClamp;
        case RHIStencilOp::Invert:    return MTL::StencilOperationInvert;
        case RHIStencilOp::IncrWrap:  return MTL::StencilOperationIncrementWrap;
        case RHIStencilOp::DecrWrap:  return MTL::StencilOperationDecrementWrap;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHIBlend blend){
        switch(blend){
        case RHIBlend::Zero:           return MTL::BlendFactorZero;
        case RHIBlend::One:            return MTL::BlendFactorOne;
        case RHIBlend::SrcColor:       return MTL::BlendFactorSourceColor;
        case RHIBlend::InvSrcColor:    return MTL::BlendFactorOneMinusSourceColor;
        case RHIBlend::SrcAlpha:       return MTL::BlendFactorSourceAlpha;
        case RHIBlend::InvSrcAlpha:    return MTL::BlendFactorOneMinusSourceAlpha;
        case RHIBlend::DestAlpha:      return MTL::BlendFactorDestinationAlpha;
        case RHIBlend::InvDestAlpha:   return MTL::BlendFactorOneMinusDestinationAlpha;
        case RHIBlend::DestColor:      return MTL::BlendFactorDestinationColor;
        case RHIBlend::InvDestColor:   return MTL::BlendFactorOneMinusDestinationColor;
        case RHIBlend::SrcAlphaSat:    return MTL::BlendFactorSourceAlphaSaturated;
        case RHIBlend::BlendFactor:    return MTL::BlendFactorBlendColor;
        case RHIBlend::InvBlendFactor: return MTL::BlendFactorOneMinusBlendColor;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHIBlendOp op){
        switch(op){
        case RHIBlendOp::Add:             return MTL::BlendOperationAdd;
        case RHIBlendOp::Subtract:        return MTL::BlendOperationSubtract;
        case RHIBlendOp::ReverseSubtract: return MTL::BlendOperationReverseSubtract;
        case RHIBlendOp::Min:             return MTL::BlendOperationMin;
        case RHIBlendOp::Max:             return MTL::BlendOperationMax;
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
        switch(access){
        case MTL::BindingAccessReadOnly:  return RHIBindingAccess::ReadOnly;
        case MTL::BindingAccessReadWrite: return RHIBindingAccess::ReadWrite;
        case MTL::BindingAccessWriteOnly: return RHIBindingAccess::WriteOnly;
        default:
            std::unreachable();
        }
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
        MTL::RenderPipelineState* renderPipeline = nullptr;
        MTL::DepthStencilState* depthStencilState = nullptr;
        RHIRasterizerState rasterizerState{};
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;

        RHIGraphicsBindingInfo bindingInfo;

        const std::string debugName;

    public:
        MetalGraphicsPipelineState(
            MTL::Device* device,
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {
            AutoreleasePoolScope _;

            // Shaders
            auto vs = static_cast<MetalShader*>(desc.vertexShader);
            auto ps = static_cast<MetalShader*>(desc.pixelShader);
            if(vs == nullptr || ps == nullptr){
                throw std::runtime_error("Vertex shader or Fragment shader is null");
            }

            auto pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
            pipelineDesc->setVertexFunction(vs->get());
            pipelineDesc->setFragmentFunction(ps->get());

            // Vertex Layout
            if(desc.vertexLayout.elementCount > 0 && desc.vertexLayout.elements){
                auto vertexDesc = MTL::VertexDescriptor::alloc()->init();
                size_t stride = 0;

                for(uint32_t i = 0; i < desc.vertexLayout.elementCount; ++i){
                    const auto& elem = desc.vertexLayout.elements[i];
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
                    convertTextureFormat(desc.renderTargetFormats[i])
                );

                const auto& rtBlend = desc.blend.renderTargets[i];
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

                MTL::ColorWriteMask writeMask = MTL::ColorWriteMaskNone;
                if(rtBlend.renderTargetWriteMask & 0x1) writeMask |= MTL::ColorWriteMaskRed;
                if(rtBlend.renderTargetWriteMask & 0x2) writeMask |= MTL::ColorWriteMaskGreen;
                if(rtBlend.renderTargetWriteMask & 0x4) writeMask |= MTL::ColorWriteMaskBlue;
                if(rtBlend.renderTargetWriteMask & 0x8) writeMask |= MTL::ColorWriteMaskAlpha;
                colorAttach->setWriteMask(writeMask);
            }

            // Depth Stencil Format
            if(desc.depthStencil.has_value()){
                auto depthStencilFormat = desc.depthStencil->format;
                CROWY_ASSERT(depthStencilFormat != RHITextureFormat::Unknown);

                pipelineDesc->setDepthAttachmentPixelFormat(
                    convertTextureFormat(depthStencilFormat)
                );
            }

            // Alpha to Coverage
            pipelineDesc->setAlphaToCoverageEnabled(
                desc.blend.alphaToCoverageEnable
            );

            MTL::AutoreleasedRenderPipelineReflection refl = nullptr;
            NS::Error* error = nullptr;
            renderPipeline = device->newRenderPipelineState(
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
                createDepthStencilState(device, *desc.depthStencil);
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
            MTL::Device* device, 
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

            depthStencilState = device->newDepthStencilState(dsDesc);
            dsDesc->release();
        }
    };

    class MetalComputePipelineState
#ifndef USE_STATIC_RHI
        : public RHIComputePipelineState
#endif
    {
    private:
        MTL::ComputePipelineState* computePipeline = nullptr;
        MTL::Size threadsPerThreadgroup = {0, 0, 0};

        RHIComputeBindingInfo bindingInfo;

        const std::string debugName;

    public:
        MetalComputePipelineState(
            MTL::Device* device,
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {
            AutoreleasePoolScope _;

            auto cs = static_cast<MetalShader*>(desc.computeShader);
            if(cs == nullptr){
                throw std::runtime_error("Compute shader is null");
            }

            auto pipelineDesc = MTL::ComputePipelineDescriptor::alloc()->init();
            pipelineDesc->setComputeFunction(cs->get());

            MTL::AutoreleasedComputePipelineReflection refl = nullptr;
            NS::Error* error = nullptr;
            computePipeline = device->newComputePipelineState(
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