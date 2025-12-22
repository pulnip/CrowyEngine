#pragma once

namespace Crowy
{
    class RHIDevice;
    struct ResourceHub;

    struct LoadContext{
        RHIDevice& device;
        ResourceHub& hub;
    };
}