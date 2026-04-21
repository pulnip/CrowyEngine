#include "Log.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "FramePacer.hpp"

namespace Crowy
{
    // Helper class for managing per-frame fences
    // Used for triple buffering to synchronize CPU and GPU work
    class RHIFrameFenceManager{
    private:
        RHIDevice* device;
        RHIFencePtr fence;
        uint64_t currentFenceValue = 0;
        uint32_t currentFrame = 0;

    public:
        RHIFrameFenceManager(RHIDevice* device) noexcept
            : device(device)
            , fence(device->createFence(0))
        {
            LOG_INFO(LOG_RHI,
                "Created frame fence manager with {} frames in flight",
                RHI_FRAMES_IN_FLIGHT
            );
        }

        ~RHIFrameFenceManager(){
            // Wait for all fences before destruction
            waitForAll();
        }

        // Begin a new frame
        // Waits for N-2 frame to ensure GPU is done with it
        void beginFrame() noexcept{
            if(currentFenceValue >= RHI_FRAMES_IN_FLIGHT){
                auto waitValue = currentFenceValue - RHI_FRAMES_IN_FLIGHT;
                fence->waitCPU(waitValue);
            }
        }

        // End the current frame
        // Signals the fence so GPU can indicate when work is done
        void endFrame() noexcept{
            // Increment fence value for next wait
            ++currentFenceValue;

            // Move to next frame
            currentFrame = (currentFrame + 1) % RHI_FRAMES_IN_FLIGHT;
        }

        // Wait for all frames to complete
        void waitForAll() noexcept{
            if(currentFenceValue > 0){
                fence->waitCPU(currentFenceValue);
            }

            LOG_INFO(LOG_RHI,
                "Waited for all {} frames to complete", RHI_FRAMES_IN_FLIGHT
            );
        }

        // Get current frame index
        uint32_t getCurrentFrameIndex() const noexcept{
            return currentFrame;
        }

        // Get fence for current frame
        RHIFence* getCurrentFence() const noexcept{
            return fence.get();
        }

        // Get fence value for current frame
        uint64_t getCurrentFenceValue() const noexcept{
            return currentFenceValue;
        }
    };

    struct FramePacer::Impl{
        RHIDevice* device;
        RHIFrameScopePtr scope = nullptr;
        RHIFrameFenceManager fenceManager;

        uint64_t frameNumber = 0;
        std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameTime = std::chrono::high_resolution_clock::now();
        double deltaTime = 0.0;
        double fps = 0.0;
        double frameTimeAccum = 0.0;
        uint32_t frameCount = 0;

        Impl(RHIDevice* device) noexcept
            : device(device), fenceManager(device){}

        ~Impl() = default;

        bool beginFrame() noexcept{
            scope = device->createFrameScope();

            // Wait for oldest frame to complete
            fenceManager.beginFrame();

            // Update timing
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentTime - lastFrameTime;
            deltaTime = elapsed.count();
            lastFrameTime = currentTime;

            // Update FPS counter
            frameTimeAccum += deltaTime;
            ++frameCount;

            if(frameTimeAccum >= 1.0){
                fps = static_cast<double>(frameCount) / frameTimeAccum;
                frameTimeAccum = 0.0;
                frameCount = 0;

                LOG_DEBUG(LOG_RHI,
                    "FPS: {:.1f}, Frame Time: {:.2f}ms",
                    fps, deltaTime * 1000.0
                );
            }

            ++frameNumber;
            return true;
        }

        void endFrame() noexcept{
            scope = nullptr;

            // Signal fence for this frame
            fenceManager.endFrame();
        }

        void waitForIdle() noexcept{
            fenceManager.waitForAll();
            LOG_INFO(LOG_RHI, "Frame pacer idle");
        }

        uint32_t getCurrentFrameIndex() const noexcept{
            return fenceManager.getCurrentFrameIndex();
        }

        uint64_t getFrameNumber() const noexcept{ return frameNumber; }
        double getDeltaTime() const noexcept{ return deltaTime; }
        double getFPS() const noexcept{ return fps; }
        double getFrameTimeMs() const noexcept{ return deltaTime * 1000.0; }

        RHIFence* getCurrentFence() noexcept{
            return fenceManager.getCurrentFence();
        }
        const RHIFence* getCurrentFence() const noexcept{
            return fenceManager.getCurrentFence();
        }

        uint64_t getNextFenceValue() const noexcept{
            // Return the next fence value that will be used after endFrame()
            return fenceManager.getCurrentFenceValue() + 1;
        }
    };

    FramePacer::FramePacer(RHIDevice* device) noexcept
        :impl(std::make_unique<Impl>(device)){}

    FramePacer::~FramePacer() = default;

    bool FramePacer::beginFrame() noexcept{
        return impl->beginFrame();
    }

    void FramePacer::endFrame() noexcept{
        impl->endFrame();
    }

    void FramePacer::waitForIdle() noexcept{
        impl->waitForIdle();
    }

    uint32_t FramePacer::getCurrentFrameIndex() const noexcept{
        return impl->getCurrentFrameIndex();
    }
    uint64_t FramePacer::getFrameNumber() const noexcept{
        return impl->getFrameNumber();
    }
    double FramePacer::getDeltaTime() const noexcept{
        return impl->getDeltaTime();
    }
    double FramePacer::getFPS() const noexcept{
        return impl->getFPS();
    }
    double FramePacer::getFrameTimeMs() const noexcept{
        return impl->getFrameTimeMs();
    }

    RHIFence* FramePacer::getCurrentFence() noexcept{
        return impl->getCurrentFence();
    }
    const RHIFence* FramePacer::getCurrentFence() const noexcept{
        return impl->getCurrentFence();
    }

    uint64_t FramePacer::getNextFenceValue() const noexcept{
        return impl->getNextFenceValue();
    }
}
