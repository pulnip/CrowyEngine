#include <array>
#include <span>
#include "Log.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "FramePacer.hpp"

constexpr auto RHI_FRAMES_IN_FLIGHT = 3;

namespace Crowy
{
    // Helper class for managing per-frame fences
    // Used for triple buffering to synchronize CPU and GPU work
    class RHIFrameFenceManager{
    private:
        RHIDevice* device;
        RHIFencePtr fence;
        uint64_t currentFenceValue;
        uint32_t currentFrame = 0;

    public:
        RHIFrameFenceManager(RHIDevice* device)
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
        void beginFrame(){
            if(currentFenceValue >= RHI_FRAMES_IN_FLIGHT){
                auto waitValue = currentFenceValue - RHI_FRAMES_IN_FLIGHT;
                fence->waitCPU(waitValue);
            }
        }

        // End the current frame
        // Signals the fence so GPU can indicate when work is done
        void endFrame(){
            // Increment fence value for next wait
            ++currentFenceValue;

            // Move to next frame
            currentFrame = (currentFrame + 1) % RHI_FRAMES_IN_FLIGHT;
        }

        // Wait for all frames to complete
        void waitForAll(){
            if(currentFenceValue > 0){
                fence->waitCPU(currentFenceValue);
            }

            LOG_INFO(LOG_RHI,
                "Waited for all {} frames to complete", RHI_FRAMES_IN_FLIGHT
            );
        }

        // Get current frame index
        uint32_t getCurrentFrameIndex() const{
            return currentFrame;
        }

        // Get fence for current frame
        RHIFence* getCurrentFence() const{
            return fence.get();
        }

        // Get fence value for current frame
        uint64_t getCurrentFenceValue() const{
            return currentFenceValue;
        }
    };

    struct FramePacer::Impl{
        RHIDevice* device;
        RHIFrameFenceManager fenceManager;

        uint64_t frameNumber = 0;
        std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameTime = std::chrono::high_resolution_clock::now();
        double deltaTime = 0.0;
        double fps = 0.0;
        double frameTimeAccum = 0.0;
        uint32_t frameCount = 0;

        Impl(RHIDevice* device)
            : device(device), fenceManager(device){}

        ~Impl() = default;

        bool beginFrame(){
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

        void endFrame(){
            // Signal fence for this frame
            fenceManager.endFrame();
        }

        void waitForIdle(){
            fenceManager.waitForAll();
            LOG_INFO(LOG_RHI, "Frame pacer idle");
        }

        uint32_t getCurrentFrameIndex() const{
            return fenceManager.getCurrentFrameIndex();
        }

        uint64_t getFrameNumber() const{ return frameNumber; }
        double getDeltaTime() const{ return deltaTime; }
        double getFPS() const{ return fps; }
        double getFrameTimeMs() const{ return deltaTime * 1000.0; }

        RHIFence* getCurrentFence(){
            return fenceManager.getCurrentFence();
        }
        const RHIFence* getCurrentFence() const{
            return fenceManager.getCurrentFence();
        }
    };

    FramePacer::FramePacer(RHIDevice* device)
        : impl(std::make_unique<Impl>(device)){}

    FramePacer::~FramePacer() = default;

    bool FramePacer::beginFrame(){
        return impl->beginFrame();
    }

    void FramePacer::endFrame(){
        impl->endFrame();
    }

    void FramePacer::waitForIdle(){
        impl->waitForIdle();
    }

    uint32_t FramePacer::getCurrentFrameIndex() const{
        return impl->getCurrentFrameIndex();
    }
    uint64_t FramePacer::getFrameNumber() const{
        return impl->getFrameNumber();
    }
    double FramePacer::getDeltaTime() const{
        return impl->getDeltaTime();
    }
    double FramePacer::getFPS() const{
        return impl->getFPS();
    }
    double FramePacer::getFrameTimeMs() const{
        return impl->getFrameTimeMs();
    }

    RHIFence* FramePacer::getCurrentFence(){
        return impl->getCurrentFence();
    }
    const RHIFence* FramePacer::getCurrentFence() const{
        return impl->getCurrentFence();
    }
}
