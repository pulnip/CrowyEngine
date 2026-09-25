#include <cstdlib>
#include <utility>
#include "LogLocal.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    namespace{
        constexpr u64 DefaultDumpFrame = 60;
    }

    RHISwapchain::RHISwapchain(RHIPixelFormat format)
        : format(format)
    {
        if(const char* path = std::getenv("CROWY_DUMP_FRAME")){
            dumpPath = path;
            dumpAtFrame = DefaultDumpFrame;
            if(const char* at = std::getenv("CROWY_DUMP_FRAME_AT")){
                if(const int parsed = std::atoi(at); parsed > 0)
                    dumpAtFrame = static_cast<u64>(parsed);
            }
            dumpState->store(FrameDumpState::Pending, std::memory_order_release);
        }
    }

    bool RHISwapchain::RequestFrameDump(Str path){
        if(GetFrameDumpState() == FrameDumpState::Pending)
            return false;

        dumpPath = std::move(path);
        dumpAtFrame = 0;
        dumpState->store(FrameDumpState::Pending, std::memory_order_release);

        return true;
    }

    std::optional<Str> RHISwapchain::TakeFrameDump(u64 frame) noexcept{
        if(GetFrameDumpState() != FrameDumpState::Pending || dumpPath.empty())
            return std::nullopt;
        // at or after, not equal: a frame without a drawable presents nothing
        if(dumpAtFrame != 0 && frame < dumpAtFrame)
            return std::nullopt;

        return std::exchange(dumpPath, Str{});
    }

    std::function<void(bool)> RHISwapchain::FrameDumpCompletion() const{
        return [state = dumpState](bool written){
            state->store(
                written ? FrameDumpState::Written : FrameDumpState::Failed,
                std::memory_order_release
            );
        };
    }
}
