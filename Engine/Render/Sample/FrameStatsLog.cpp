#include "FrameStatsLog.hpp"

#include <format>
#include <utility>

#include "DOM.hpp"
#include "FrameSelector.hpp"
#include "RHIFrameStats.hpp"

namespace Crowy
{
    namespace
    {
        // every number describes stats.report.frame, one frame that ended,
        // except gpu: the newest GPU time then, with what that frame drew
        // from gpuFrame; the time alone once gpuFrame has aged out
        DOM::Value statsToDom(
            const FrameStats& stats,
            const FrameStats* gpuFrame
        ) {
            constexpr auto MsPerSecond = 1000.0;

            DOM::Table result;
            result.emplace(
                "frame",
                DOM::Value(static_cast<i64>(stats.report.frame))
            );
            result.emplace(
                "primitives",
                DOM::Value(static_cast<i64>(stats.primitives))
            );
            result.emplace(
                "visiblePrimitives",
                DOM::Value(static_cast<i64>(stats.visiblePrimitives))
            );
            result.emplace(
                "triangles",
                DOM::Value(static_cast<i64>(stats.triangles))
            );
            result.emplace("draws", DOM::Value(static_cast<i64>(stats.draws)));
            result.emplace("runs", DOM::Value(static_cast<i64>(stats.runs)));
            result.emplace(
                "pipelines",
                DOM::Value(static_cast<i64>(stats.pipelines))
            );
            result.emplace(
                "instrumented",
                DOM::Value(static_cast<bool>(CROWY_FRAME_STATS))
            );

            DOM::Array passes;
            for(const auto& pass: stats.passes) {
                DOM::Table row;
                row.emplace("name", DOM::Value(pass.name));
                row.emplace("draws", DOM::Value(static_cast<i64>(pass.draws)));
                row.emplace("runs", DOM::Value(static_cast<i64>(pass.runs)));
                row.emplace(
                    "triangles",
                    DOM::Value(static_cast<i64>(pass.triangles))
                );
                row.emplace(
                    "barrierEdges",
                    DOM::Value(static_cast<i64>(pass.barrierEdges))
                );
                passes.push_back(DOM::Value(std::move(row)));
            }
            result.emplace("passes", DOM::Value(std::move(passes)));

            DOM::Table cpu;
            for(usize i = 0; i < NUM_FRAME_SECTION; ++i) {
                cpu.emplace(
                    Str{ToString(static_cast<FrameSection>(i))} + "Ms",
                    DOM::Value(stats.report.seconds[i] * MsPerSecond)
                );
            }
            result.emplace("cpu", DOM::Value(std::move(cpu)));

            const auto& s = stats.report.rhi;
            DOM::Table rhi;
            rhi.emplace(
                "commandListBegins",
                DOM::Value(static_cast<i64>(s.commandListBeginCount))
            );
            rhi.emplace(
                "commandListCreates",
                DOM::Value(static_cast<i64>(s.commandListCreateCount))
            );
            rhi.emplace(
                "renderPasses",
                DOM::Value(static_cast<i64>(s.renderPassCount))
            );
            rhi.emplace(
                "computePasses",
                DOM::Value(static_cast<i64>(s.computePassCount))
            );
            rhi.emplace(
                "blitPasses",
                DOM::Value(static_cast<i64>(s.blitPassCount))
            );
            rhi.emplace(
                "directDraws",
                DOM::Value(static_cast<i64>(s.drawCount))
            );
            rhi.emplace(
                "indirectBatches",
                DOM::Value(static_cast<i64>(s.indirectBatchCount))
            );
            rhi.emplace(
                "indirectDraws",
                DOM::Value(static_cast<i64>(s.indirectDrawCount))
            );
            rhi.emplace(
                "dispatches",
                DOM::Value(static_cast<i64>(s.dispatchCount))
            );
            rhi.emplace("copies", DOM::Value(static_cast<i64>(s.copyCount)));
            rhi.emplace(
                "pipelineSets",
                DOM::Value(static_cast<i64>(s.pipelineSetCount))
            );
            rhi.emplace(
                "constantBufferSets",
                DOM::Value(static_cast<i64>(s.constantBufferSetCount))
            );
            rhi.emplace(
                "pushConstantSets",
                DOM::Value(static_cast<i64>(s.pushConstantSetCount))
            );
            rhi.emplace(
                "barrierEdges",
                DOM::Value(static_cast<i64>(s.barrierEdgeCount))
            );
            result.emplace("rhi", DOM::Value(std::move(rhi)));

            if(!stats.report.gpu) {
                result.emplace("gpu", DOM::Value());

                return DOM::Value(std::move(result));
            }

            const auto& time = *stats.report.gpu;
            DOM::Table gpu;
            gpu.emplace("frame", DOM::Value(static_cast<i64>(time.frame)));
            gpu.emplace("frameMs", DOM::Value(time.seconds * MsPerSecond));
            if(gpuFrame != nullptr) {
                const auto& drawn = gpuFrame->report.rhi;
                gpu.emplace(
                    "directDraws",
                    DOM::Value(static_cast<i64>(drawn.drawCount))
                );
                gpu.emplace(
                    "indirectDraws",
                    DOM::Value(static_cast<i64>(drawn.indirectDrawCount))
                );
                gpu.emplace(
                    "triangles",
                    DOM::Value(static_cast<i64>(gpuFrame->triangles))
                );
            }
            result.emplace("gpu", DOM::Value(std::move(gpu)));

            return DOM::Value(std::move(result));
        }
    }

    void FrameStatsLog::Begin(u32 visiblePrimitives) {
        recorded = FrameStats{};
        recorded.visiblePrimitives = visiblePrimitives;
    }

    void FrameStatsLog::AddPasses(std::span<const PassStats> passes) {
        recorded.passes.assign(passes.begin(), passes.end());
        for(const auto& pass: passes) {
            recorded.triangles += pass.triangles;
            recorded.draws += pass.draws;
            recorded.runs += pass.runs;
        }
    }

    void FrameStatsLog::End(
        u64 frame,
        const FrameReport& report,
        usize primitives,
        usize pipelines
    ) {
        auto stats = recorded;
        stats.report = report;
        stats.primitives = primitives;
        stats.pipelines = pipelines;
        history.Push(frame, stats);
    }

    void FrameStatsLog::RegisterVerbs(CommandPort& port, LastFrame lastFrame) {
        // verbs drain before the next frame records, so the newest is the
        // last to finish
        port.RegisterVerb(
            "read_stats",
            withSelectorKeys({}),
            [this,
             lastFrame =
                 std::move(lastFrame)](const DOM::Value& args, Reply reply) {
                const auto selector = parseFrameSelector(args);
                if(!selector.error.empty()) {
                    reply.Error(selector.error);

                    return;
                }
                if(selector.frames.empty()) {
                    const auto& last = Last();
                    reply.Ok(statsToDom(last, gpuFrameOf(last)));

                    return;
                }

                DOM::Array frames;
                frames.reserve(selector.frames.size());
                for(const auto frame: selector.frames) {
                    if(frame > lastFrame()) {
                        reply.Error(
                            std::format(
                                "frame {} has not ended (the last frame is {})",
                                frame,
                                lastFrame()
                            )
                        );

                        return;
                    }

                    const auto* stats = history.Find(frame);
                    if(stats == nullptr) {
                        reply.Error(
                            std::format(
                                "frame {} is not in the history (it keeps "
                                "frames {}..{})",
                                frame,
                                history.Oldest(),
                                history.Newest()
                            )
                        );

                        return;
                    }
                    frames.push_back(statsToDom(*stats, gpuFrameOf(*stats)));
                }

                DOM::Table result;
                result.emplace("frames", DOM::Value(std::move(frames)));
                reply.Ok(DOM::Value(std::move(result)));
            }
        );
    }

    const FrameStats& FrameStatsLog::Last() const noexcept {
        static const FrameStats none;
        const auto* newest = history.Find(history.Newest());

        return newest != nullptr ? *newest : none;
    }

    const FrameStats* FrameStatsLog::gpuFrameOf(
        const FrameStats& stats
    ) const noexcept {
        return stats.report.gpu ? history.Find(stats.report.gpu->frame)
                                : nullptr;
    }
}
