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
        std::array<RHIFencePtr, RHI_FRAMES_IN_FLIGHT> fences;
        std::array<uint64_t, RHI_FRAMES_IN_FLIGHT> fenceValues;
        uint32_t currentFrame;

    public:
        RHIFrameFenceManager(RHIDevice* device)
            :device(device), currentFrame(0)
        {
            // Create fences for each frame in flight
            for(int i = 0; i < RHI_FRAMES_IN_FLIGHT; ++i){
                fences[i] = device->createFence(0);
                fenceValues[i] = 0;
            }

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
        // Waits for the fence of the oldest frame to ensure GPU is done with it
        void beginFrame(){
            // Get the fence for the current frame
            uint64_t targetValue = fenceValues[currentFrame];

            // Wait for GPU to finish with this frame's resources
            if(targetValue > 0){
                fences[currentFrame]->waitFor(targetValue);
            }
        }

        // End the current frame
        // Signals the fence so GPU can indicate when work is done
        void endFrame(){
            // Increment fence value for next wait
            ++fenceValues[currentFrame];

            // Signal the fence with the new value
            fences[currentFrame]->signal(fenceValues[currentFrame]);

            // Move to next frame
            currentFrame = (currentFrame + 1) % RHI_FRAMES_IN_FLIGHT;
        }

        // Wait for all frames to complete
        void waitForAll(){
            for(int i = 0; i < RHI_FRAMES_IN_FLIGHT; ++i){
                if(fenceValues[i] > 0){
                    fences[i]->waitFor(fenceValues[i]);
                }
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
            return fences[currentFrame].get();
        }

        // Get fence value for current frame
        uint64_t getCurrentFenceValue() const{
            return fenceValues[currentFrame];
        }
    };

    // Simple fence wrapper for one-shot synchronization
    class RHISyncFence{
    private:
        RHIDevice* device;
        RHIFencePtr fence;
        uint64_t nextValue;
        uint64_t lastSignaledValue = 0;

    public:
        explicit RHISyncFence(RHIDevice* device, uint64_t initialValue = 0)
            : device(device)
            , fence(device->createFence(initialValue))
            , nextValue(initialValue + 1){}

        ~RHISyncFence() = default;

        // Signal the fence
        void signal(){
            fence->signal(nextValue);
            lastSignaledValue = nextValue;
            ++nextValue;
        }

        // Wait for the fence to reach the last signaled value
        void wait(){
            if(lastSignaledValue > 0){
                fence->waitFor(lastSignaledValue);
            }
        }

        void waitForValue(uint64_t value){
            fence->waitFor(value);
        }

        // Check if fence has reached a value (non-blocking)
        bool isComplete(uint64_t value) const{
            return fence->getValue() >= value;
        }

        // Check if last signaled value is complete
        bool isComplete() const{
            return lastSignaledValue > 0 &&
                   isComplete(lastSignaledValue);
        }

        uint64_t getLastSignaledValue() const {
            return lastSignaledValue;
        }

        RHIFence* getHandle(){ return fence.get(); }
        const RHIFence* getHandle() const{ return fence.get(); }
    };

    // Fence helpers for common synchronization patterns

    // Create and immediately signal a fence
    inline RHIFencePtr createSignaledFence(
        RHIDevice* device, uint64_t value = 1
    ){
        auto fence = device->createFence(value);
        fence->signal(value);

        return fence;
    }

    // Wait for multiple fences
    inline void waitForFences(
        std::span<RHIFence*> fences,
        std::span<const uint64_t> values
    ){
        if(fences.size() != values.size()){
            LOG_ERROR(LOG_RHI,
                "Fence count mismatch in waitForFences"
            );
            return;
        }

        for(int i = 0; i < fences.size(); ++i){
            fences[i]->waitFor(values[i]);
        }
    }

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
            frameCount++;

            if(frameTimeAccum >= 1.0){
                fps = static_cast<double>(frameCount) / frameTimeAccum;
                frameTimeAccum = 0.0;
                frameCount = 0;

                LOG_DEBUG(LOG_RHI,
                    "FPS: {:.1f}, Frame Time: {:.2f}ms",
                    fps, deltaTime * 1000.0
                );
            }

            frameNumber++;
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
