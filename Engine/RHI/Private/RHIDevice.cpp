#include <utility>
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    void RHIDevice::Retire(RHIBufferRAII buffer){
        if(buffer == nullptr)
            return;

        DeferRetire([buffer = std::move(buffer)]{});
    }

    void RHIDevice::Retire(RHITextureRAII texture){
        if(texture == nullptr)
            return;

        DeferRetire([texture = std::move(texture)]{});
    }

    void RHIDevice::Retire(RHIGraphicsPipelineStateRAII state){
        if(state == nullptr)
            return;

        DeferRetire([state = std::move(state)]{});
    }
}
