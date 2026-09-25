#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <utility>
#include "Assert.hpp"
#include "LogLocal.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    namespace{
        constexpr u64 DefaultDumpFrame = 60;
        // far past any BMP write; a handler that never runs must not hang
        // the exit
        constexpr auto DumpJoinLimit = std::chrono::seconds(5);
    }

    RHISwapchain::RHISwapchain(RHIPixelFormat format)
        : format(format)
    {
        if(const char* path = std::getenv("CROWY_DUMP_FRAME")){
            auto frame = DefaultDumpFrame;
            if(const char* at = std::getenv("CROWY_DUMP_FRAME_AT")){
                if(const int parsed = std::atoi(at); parsed > 0)
                    frame = static_cast<u64>(parsed);
            }

            RequestFrameDump(path, frame);
        }
    }

    RHISwapchain::~RHISwapchain(){
        // a Metal handler runs once its GPU work is done, which the loop's
        // last WaitForIdle has already waited for, so this is short
        const auto deadline = std::chrono::steady_clock::now() + DumpJoinLimit;
        for(const auto& dump: dumpsInFlight){
            while(dump.state->load(std::memory_order_acquire) == FrameDumpState::Pending){
                if(std::chrono::steady_clock::now() > deadline){
                    LOG_WARN("frame dump for frame {} to '{}' still writing at shutdown",
                        dump.outcome.requested,
                        dump.outcome.path
                    );
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        for(const auto& request: dumpRequests){
            LOG_WARN("frame dump for frame {} to '{}' never ran",
                request.frame,
                request.path
            );
        }
    }

    bool RHISwapchain::IsFrameDumpQueued(u64 frame, StrView path) const noexcept{
        const bool queued = std::ranges::any_of(dumpRequests,
            [&](const FrameDumpRequest& request){
                return request.frame == frame || request.path == path;
            }
        );
        const bool writing = std::ranges::any_of(dumpsInFlight,
            [&](const FrameDumpInFlight& dump){
                return dump.outcome.requested == frame || dump.outcome.path == path;
            }
        );

        return queued || writing;
    }

    void RHISwapchain::RequestFrameDump(Str path, u64 frame){
        CROWY_ASSERT(!IsFrameDumpQueued(frame, path),
            "frame {} or '{}' already has a dump queued", frame, path
        );
        CROWY_ASSERT(PendingFrameDumps() < MaxFrameDumps,
            "more than {} frame dumps pending", MaxFrameDumps
        );

        const auto at = std::ranges::upper_bound(
            dumpRequests, frame, {}, &FrameDumpRequest::frame
        );
        dumpRequests.insert(at, FrameDumpRequest{
            .frame = frame,
            .path = std::move(path)
        });
    }

    u32 RHISwapchain::PendingFrameDumps() const noexcept{
        return static_cast<u32>(dumpRequests.size() + dumpsInFlight.size());
    }

    std::vector<FrameDumpOutcome> RHISwapchain::TakeFrameDumpOutcomes(){
        if(dumpsInFlight.empty())
            return {};

        // one load per dump: a completion may land between two looks
        std::vector<FrameDumpOutcome> finished;
        std::vector<FrameDumpInFlight> writing;
        for(auto& dump: dumpsInFlight){
            const auto state = dump.state->load(std::memory_order_acquire);
            if(state == FrameDumpState::Pending){
                writing.push_back(std::move(dump));
                continue;
            }

            dump.outcome.written = state == FrameDumpState::Written;
            finished.push_back(std::move(dump.outcome));
        }
        dumpsInFlight = std::move(writing);

        return finished;
    }

    std::optional<RHISwapchain::FrameDumpJob> RHISwapchain::TakeFrameDump(u64 frame){
        // at or after, not equal: a frame without a drawable presents nothing
        if(dumpRequests.empty() || dumpRequests.front().frame > frame)
            return std::nullopt;

        auto request = std::move(dumpRequests.front());
        dumpRequests.erase(dumpRequests.begin());

        auto state = std::make_shared<std::atomic<FrameDumpState>>(
            FrameDumpState::Pending
        );
        dumpsInFlight.push_back(FrameDumpInFlight{
            .outcome = FrameDumpOutcome{
                .requested = request.frame,
                .presented = frame,
                .path = request.path
            },
            .state = state
        });

        return FrameDumpJob{
            .path = std::move(request.path),
            .completion = [state = std::move(state)](bool written){
                state->store(
                    written ? FrameDumpState::Written : FrameDumpState::Failed,
                    std::memory_order_release
                );
            }
        };
    }
}
