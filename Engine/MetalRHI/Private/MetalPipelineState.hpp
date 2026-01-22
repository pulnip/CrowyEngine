#pragma once

#include <utility>
#include <Metal/Metal.hpp>
#include "MetalShader.hpp"
#include "MetalUtil.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
    static MTL::VertexFormat convertVertexFormat(RHITextureFormat format){
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

    static MTL::CompareFunction convertCompareFunc(RHIComparisonFunc func){
        switch(func){
        case RHIComparisonFunc::Never:        return MTL::CompareFunctionNever;
        case RHIComparisonFunc::Less:         return MTL::CompareFunctionLess;
        case RHIComparisonFunc::Equal:        return MTL::CompareFunctionEqual;
        case RHIComparisonFunc::LessEqual:    return MTL::CompareFunctionLessEqual;
        case RHIComparisonFunc::Greater:      return MTL::CompareFunctionGreater;
        case RHIComparisonFunc::NotEqual:     return MTL::CompareFunctionNotEqual;
        case RHIComparisonFunc::GreaterEqual: return MTL::CompareFunctionGreaterEqual;
        case RHIComparisonFunc::Always:       return MTL::CompareFunctionAlways;
        default:
            std::unreachable();
        }
    }

    static MTL::BlendFactor convertBlendFactor(RHIBlend blend){
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

    static MTL::BlendOperation convertBlendOp(RHIBlendOp op){
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

    class MetalPipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    private:
        MTL::RenderPipelineState* renderPipeline = nullptr;
        MTL::ComputePipelineState* computePipeline = nullptr;
        MTL::DepthStencilState* depthStencilState = nullptr;

        RHIRasterizerState rasterizerState{};
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
        bool isCompute = false;

    public:
        MetalPipelineState(
            MTL::Device* device,
            const RHIGraphicsPipelineStateDesc& desc
        )
            : isCompute(false)
        {
            auto pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();

            // Shaders
            auto vs = static_cast<MetalShader*>(desc.vertexShader);
            auto ps = static_cast<MetalShader*>(desc.pixelShader);

            if(vs) pipelineDesc->setVertexFunction(vs->get());
            if(ps) pipelineDesc->setFragmentFunction(ps->get());

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
                        convertBlendFactor(rtBlend.srcBlend)
                    );
                    colorAttach->setDestinationRGBBlendFactor(
                        convertBlendFactor(rtBlend.dstBlend)
                    );
                    colorAttach->setRgbBlendOperation(
                        convertBlendOp(rtBlend.blendOp)
                    );
                    colorAttach->setSourceAlphaBlendFactor(
                        convertBlendFactor(rtBlend.srcBlendAlpha)
                    );
                    colorAttach->setDestinationAlphaBlendFactor(
                        convertBlendFactor(rtBlend.dstBlendAlpha)
                    );
                    colorAttach->setAlphaBlendOperation(
                        convertBlendOp(rtBlend.blendOpAlpha)
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
            if(desc.depthStencilFormat != RHITextureFormat::Unknown){
                pipelineDesc->setDepthAttachmentPixelFormat(
                    convertTextureFormat(desc.depthStencilFormat)
                );
            }

            // Alpha to Coverage
            pipelineDesc->setAlphaToCoverageEnabled(
                desc.blend.alphaToCoverageEnable
            );

            NS::Error* error = nullptr;
            renderPipeline = device->newRenderPipelineState(pipelineDesc, &error);
            pipelineDesc->release();

            if(!renderPipeline){
                throw std::runtime_error("Failed to create render pipeline state");
            }

            // NOTE. discard desc.debugName

            // Depth Stencil State
            if(desc.depthStencil.depthEnable || desc.depthStencil.stencilEnable){
                createDepthStencilState(device, desc.depthStencil);
            }

            // Store rasterizer state for command list
            rasterizerState = desc.rasterizer;
            topology = desc.topology;
        }

        MetalPipelineState(
            MTL::Device* device,
            const RHIComputePipelineStateDesc& desc
        )
            : isCompute(true)
        {
            auto cs = static_cast<MetalShader*>(desc.computeShader);
            if(!cs){
                throw std::runtime_error("Compute shader is null");
            }

            NS::Error* error = nullptr;
            computePipeline = device->newComputePipelineState(
                cs->get(), &error
            );

            if(!computePipeline){
                throw std::runtime_error("Failed to create compute pipeline state");
            }

            // NOTE. discard desc.debugName
        }

        ~MetalPipelineState(){
            if(renderPipeline) renderPipeline->release();
            if(computePipeline) computePipeline->release();
            if(depthStencilState) depthStencilState->release();
        }

        MTL::RenderPipelineState* getRenderPipeline() const{ 
            return renderPipeline; 
        }

        MTL::ComputePipelineState* getComputePipeline() const{ 
            return computePipeline; 
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

        bool isComputePipeline() const{
            return isCompute;
        }

    private:
        void createDepthStencilState(
            MTL::Device* device, 
            const RHIDepthStencilState& desc
        ){
            auto dsDesc = MTL::DepthStencilDescriptor::alloc()->init();
            
            dsDesc->setDepthWriteEnabled(desc.depthWriteEnable);
            
            if(desc.depthEnable){
                dsDesc->setDepthCompareFunction(
                    convertCompareFunc(desc.depthFunc)
                );
            } else {
                dsDesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
            }

            // TODO: Stencil state 설정

            depthStencilState = device->newDepthStencilState(dsDesc);
            dsDesc->release();
        }
    };
}