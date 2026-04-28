#include "FramePacer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    FramePacerRAII RHIDevice::createFramePacer() noexcept{
        return std::make_unique<FramePacer>(*this);
    }
}