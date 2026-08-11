#include "FrameProfiler.hpp"

namespace Crowy
{
    CStr ToString(FrameSection section) noexcept{
        switch(section){
        case FrameSection::Events:    return "events";
        case FrameSection::Update:    return "update";
        case FrameSection::FenceWait: return "fenceWait";
        case FrameSection::Acquire:   return "acquire";
        case FrameSection::Record:    return "record";
        case FrameSection::Submit:    return "submit";
        case FrameSection::Frame:     return "frame";
        case FrameSection::Unknown:   break;
        }
        return "unknown";
    }
}

#if CROWY_BENCHMARK

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include "LogLocal.hpp"

namespace Crowy
{
    namespace{
        constexpr f64 SECONDS_TO_MS = 1000.0;
        constexpr f64 MS_TO_US = 1000.0;

        // the same gate DX12Device puts its debug layer behind
    #if defined(_DEBUG) || !defined(NDEBUG)
        constexpr CStr BUILD_KIND = "Debug";
        constexpr CStr DEBUG_LAYER = "on";
    #else
        constexpr CStr BUILD_KIND = "Release";
        constexpr CStr DEBUG_LAYER = "off";
    #endif

    #if defined(_WIN32)
        constexpr CStr BACKEND = "D3D12";
    #elif defined(__APPLE__)
        constexpr CStr BACKEND = "Metal";
    #else
        constexpr CStr BACKEND = "unknown";
    #endif

        struct Summary{
            f64 p50 = 0.0, p95 = 0.0, p99 = 0.0, max = 0.0, mean = 0.0;
        };

        // partitions the range; every call stands on its own, so the order
        // these run in does not matter
        f64 percentileOf(std::vector<f64>& values, f64 ratio){
            auto index = static_cast<usize>(ratio * (values.size() - 1));
            auto nth = values.begin() + index;

            std::nth_element(values.begin(), nth, values.end());
            return *nth;
        }

        Summary summarize(std::vector<f64>& values){
            if(values.empty()) [[unlikely]]
                return {};

            f64 total = 0.0;
            for(auto value: values)
                total += value;

            return Summary{
                .p50 = percentileOf(values, 0.50),
                .p95 = percentileOf(values, 0.95),
                .p99 = percentileOf(values, 0.99),
                .max = percentileOf(values, 1.00),
                .mean = total / static_cast<f64>(values.size())
            };
        }

        bool openOutput(const Str& path, std::ofstream& out){
            if(path.empty())
                return false;

            std::error_code error;
            auto parent = std::filesystem::path(path).parent_path();
            if(!parent.empty())
                std::filesystem::create_directories(parent, error);

            out.open(path);
            if(!out){
                LOG_WARN("cannot write the benchmark output '{}'", path);
                return false;
            }

            return true;
        }
    }

    FrameProfiler::FrameProfiler(const RuntimeConfig& runtimeConfig)
        : config(runtimeConfig.benchmark)
        , title(runtimeConfig.window.title)
        , width(runtimeConfig.window.width)
        , height(runtimeConfig.window.height)
        , vsync(runtimeConfig.window.vsync)
    {
        if(config.enabled)
            records.reserve(config.measureFrames);
    }

    void FrameProfiler::BeginFrame() noexcept{
        current.fill(0.0);
        frameStart = Clock::now();
    }

    void FrameProfiler::Accumulate(FrameSection section, f64 seconds) noexcept{
        current[static_cast<usize>(section)] += seconds;
    }

    void FrameProfiler::EndFrame(
        const RHIFrameStats& stats,
        f64 fenceWaitSeconds
    ) noexcept{
        // the scope covering BeginFrame() swallowed the fence wait, and the
        // two are worth telling apart: one is setup, the other is the GPU
        auto& acquire = current[static_cast<usize>(FrameSection::Acquire)];
        acquire = std::max(0.0, acquire - fenceWaitSeconds);

        current[static_cast<usize>(FrameSection::FenceWait)] = fenceWaitSeconds;
        current[static_cast<usize>(FrameSection::Frame)] =
            std::chrono::duration<f64>(Clock::now() - frameStart).count();

        if(IsMeasuring() && records.size() < config.measureFrames){
            records.push_back(FrameRecord{
                .frameNumber = frameNumber,
                .sections = current,
                .stats = stats
            });
        }

        ++frameNumber;
    }

    bool FrameProfiler::ShouldStop() const noexcept{
        return config.enabled && records.size() >= config.measureFrames;
    }

    void FrameProfiler::WriteReport() const{
        if(!config.enabled)
            return;

        if(records.empty()){
            LOG_WARN("benchmark ended before the warmup of {} frames finished",
                config.warmupFrames
            );
            return;
        }

        // counters barely move frame to frame, so a median is a fair
        // stand-in and shrugs off the odd frame that toggled some UI
        auto medianCounter = [this](u32 RHIFrameStats::* field) -> u32{
            std::vector<u32> values;
            values.reserve(records.size());
            for(const auto& record: records)
                values.push_back(record.stats.*field);

            auto nth = values.begin() + values.size() / 2;
            std::nth_element(values.begin(), nth, values.end());
            return *nth;
        };

        std::array<Summary, NUM_FRAME_SECTION> summaries;
        for(usize i = 0; i < NUM_FRAME_SECTION; ++i){
            std::vector<f64> values;
            values.reserve(records.size());
            for(const auto& record: records)
                values.push_back(record.sections[i] * SECONDS_TO_MS);

            summaries[i] = summarize(values);
        }

        // one row per frame, for regressing record time on the draw count
        if(std::ofstream frames; openOutput(config.framePath, frames)){
            frames <<
                "frame,events_ms,update_ms,fence_wait_ms,acquire_ms,"
                "record_ms,submit_ms,frame_ms,"
                "draws,indirect_draws,dispatches,pso_sets,barrier_edges,"
                "cb_sets,push_sets,cmd_lists,cmd_lists_created\n";

            for(const auto& record: records){
                frames << std::format("{}", record.frameNumber);
                for(usize i = 0; i < NUM_FRAME_SECTION; ++i){
                    frames << std::format(",{:.6f}",
                        record.sections[i] * SECONDS_TO_MS
                    );
                }
                frames << std::format(",{},{},{},{},{},{},{},{},{}\n",
                    record.stats.drawCount,
                    record.stats.indirectDrawCount,
                    record.stats.dispatchCount,
                    record.stats.pipelineSetCount,
                    record.stats.barrierEdgeCount,
                    record.stats.constantBufferSetCount,
                    record.stats.pushConstantSetCount,
                    record.stats.commandListBeginCount,
                    record.stats.commandListCreateCount
                );
            }
        }

        std::ofstream report;
        if(!openOutput(config.reportPath, report))
            return;

        report << std::format("# {} - {}\n\n", title, BACKEND);
        report << std::format(
            "api={}  build={}  debug_layer={}  vsync={}\n"
            "warmup={}  frames={}  resolution={}x{}\n\n",
            BACKEND, BUILD_KIND, DEBUG_LAYER, vsync ? "on" : "off",
            config.warmupFrames, records.size(), width, height
        );

        report <<
            "## CPU sections (ms)\n\n"
            "| section   |   p50 |   p95 |   p99 |   max |  mean |\n"
            "|-----------|-------|-------|-------|-------|-------|\n";
        for(usize i = 0; i < NUM_FRAME_SECTION; ++i){
            const auto& summary = summaries[i];
            report << std::format(
                "| {:<9} | {:5.3f} | {:5.3f} | {:5.3f} | {:5.3f} | {:5.3f} |\n",
                ToString(static_cast<FrameSection>(i)),
                summary.p50, summary.p95, summary.p99, summary.max, summary.mean
            );
        }

        auto drawCount = medianCounter(&RHIFrameStats::drawCount);

        report << std::format(
            "\n## Per-frame RHI counters (median)\n\n"
            "- draws {}, indirect draws {}, dispatches {}\n"
            "- pipeline sets {}, constant buffer sets {}, push constant sets {}\n"
            "- barrier edges {}, copies {}\n"
            "- render passes {}, compute passes {}, blit passes {}\n"
            "- command lists {}, created this frame {}\n",
            drawCount,
            medianCounter(&RHIFrameStats::indirectDrawCount),
            medianCounter(&RHIFrameStats::dispatchCount),
            medianCounter(&RHIFrameStats::pipelineSetCount),
            medianCounter(&RHIFrameStats::constantBufferSetCount),
            medianCounter(&RHIFrameStats::pushConstantSetCount),
            medianCounter(&RHIFrameStats::barrierEdgeCount),
            medianCounter(&RHIFrameStats::copyCount),
            medianCounter(&RHIFrameStats::renderPassCount),
            medianCounter(&RHIFrameStats::computePassCount),
            medianCounter(&RHIFrameStats::blitPassCount),
            medianCounter(&RHIFrameStats::commandListBeginCount),
            medianCounter(&RHIFrameStats::commandListCreateCount)
        );

        report << "\n## Verdict\n\n";
        if(drawCount == 0){
            report << "no draws recorded, so there is no per-draw cost\n";
            return;
        }

        const auto& recordSection = summaries[
            static_cast<usize>(FrameSection::Record)
        ];
        auto perDraw = [drawCount](f64 ms){
            return ms * MS_TO_US / static_cast<f64>(drawCount);
        };

        report << std::format(
            "record CPU per draw: p50 {:.2f} us, p95 {:.2f} us\n\n"
            "Counters that scale with the draw count instead of the pass\n"
            "count are the thing to check before reading this number.\n",
            perDraw(recordSection.p50), perDraw(recordSection.p95)
        );
    }
}

#endif
