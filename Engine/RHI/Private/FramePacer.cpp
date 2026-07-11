#include "Assert.hpp"
#include "LogLocal.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "RHIFrameScope.hpp"
#include "FramePacer.hpp"

namespace Crowy
{
    // Helper class for managing per-frame fences
    // Used for triple buffering to synchronize CPU and GPU work
    class RHIFrameFenceManager{
    private:
        RHIDevice& device;
        RHIFenceRAII fence;
        u64 currentFenceValue = 0;

    public:
        RHIFrameFenceManager(RHIDevice& device)
            : device(device)
            , fence(device.CreateFence(0))
        {
            SMOL_ASSERT(fence != nullptr);

            LOG_INFO("Created frame fence manager with {} frames in flight",
                RHI_FRAMES_IN_FLIGHT
            );
        }

        ~RHIFrameFenceManager(){
            // Wait for all fences before destruction
            WaitForAll();
        }

        // Begin a new frame
        // Waits for N-2 frame to ensure GPU is done with it
        void BeginFrame(){
            if(currentFenceValue < RHI_FRAMES_IN_FLIGHT) [[unlikely]]
                return;

            auto waitValue = currentFenceValue - RHI_FRAMES_IN_FLIGHT;
            fence->WaitCPU(waitValue);
        }

        // End the current frame
        // Signals the fence so GPU can indicate when work is done
        void EndFrame(){
            // Increment fence value for next wait
            ++currentFenceValue;
        }

        // Wait for all frames to complete
        void WaitForAll(){
            if(currentFenceValue == 0) [[unlikely]]
                return;

            fence->WaitCPU(currentFenceValue);

            LOG_INFO("Waited for all {} frames to complete",
                RHI_FRAMES_IN_FLIGHT
            );
        }

        // Get fence for current frame
        RHIFence& GetCurrentFence() const noexcept{
            return *fence.get();
        }

        // Get fence value for current frame
        u64 GetCurrentFenceValue() const noexcept{
            return currentFenceValue;
        }
    };

    class FramePacer::Impl{
    private:
        RHIDevice& device;
        RHIFrameScopeRAII scope = nullptr;
        RHIFrameFenceManager fenceManager;

    public:
        Impl(RHIDevice& device)
            : device(device)
            , fenceManager(device)
        {}

        ~Impl() = default;

        bool BeginFrame(){
            scope = device.CreateFrameScope();

            // Wait for oldest frame to complete
            fenceManager.BeginFrame();

            // Update timing
            return true;
        }

        void EndFrame(){
            scope = nullptr;

            // Signal fence for this frame
            fenceManager.EndFrame();
        }

        void WaitForIdle(){
            fenceManager.WaitForAll();
            LOG_INFO("Frame pacer idle");
        }

        auto& GetCurrentFence(this auto& self) noexcept{
            return self.fenceManager.GetCurrentFence();
        }

        u64 GetNextFenceValue() const noexcept{
            // Return the next fence value that will be used after endFrame()
            return fenceManager.GetCurrentFenceValue() + 1;
        }
    };

    FramePacer::FramePacer(RHIDevice& device)
        : impl(std::make_unique<Impl>(device)){}

    FramePacer::~FramePacer() = default;

    bool FramePacer::BeginFrame(){
        return impl->BeginFrame();
    }

    void FramePacer::EndFrame(){
        impl->EndFrame();
    }

    void FramePacer::WaitForIdle(){
        impl->WaitForIdle();
    }

    RHIFence& FramePacer::GetCurrentFence() noexcept{
        return impl->GetCurrentFence();
    }
    const RHIFence& FramePacer::GetCurrentFence() const noexcept{
        return impl->GetCurrentFence();
    }

    u64 FramePacer::GetNextFenceValue() const{
        return impl->GetNextFenceValue();
    }
}
