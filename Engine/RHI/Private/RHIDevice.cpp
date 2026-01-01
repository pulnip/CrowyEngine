#include "FramePacer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    FramePacerPtr RHIDevice::createFramePacer(){
        return std::make_unique<FramePacer>(this);
    }
}