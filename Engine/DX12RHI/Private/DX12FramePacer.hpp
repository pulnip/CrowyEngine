#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    class DX12Device;

    class DX12FramePacer{
    private:
        class Impl;
        RAII<Impl> impl;

    public:
        DX12FramePacer(DX12Device& device);
        ~DX12FramePacer();

        bool BeginFrame();
        void EndFrame();

        void WaitForIdle();
    };
}
