#include "FramePacer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    FramePacerPtr RHIDevice::createFramePacer() noexcept{
        return std::make_unique<FramePacer>(this);
    }
}