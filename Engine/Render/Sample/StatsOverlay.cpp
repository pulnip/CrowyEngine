#include "StatsOverlay.hpp"

#include <imgui.h>

namespace Crowy
{
    void StatsOverlay::Draw(const RenderApp::FrameStats& stats) {
        using enum FrameSection;

        struct CpuRow {
            CStr label;
            FrameSection section;
        };

        constexpr f64 MsPerSecond = 1000.0;
        constexpr f64 Smoothing = 0.1;
        constexpr f32 Pad = 10.0f;
        constexpr std::array CpuRows{
            CpuRow{"Frame", Frame},
            CpuRow{"Events", Events},
            CpuRow{"Update", Update},
            CpuRow{"Acquire", Acquire},
            CpuRow{"Record", Record},
            CpuRow{"Submit", Submit},
            CpuRow{"Fence wait", FenceWait}
        };
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_AlwaysAutoResize;

        const auto& report = stats.report;
        // nothing measured yet, or never: an uninstrumented build's report
        // stays at frame 0
        if(report.frame == 0)
            return;

        if(report.frame != lastFrame) {
            // a gap means the overlay was hidden, and an average of frames
            // it never saw would lie
            const bool reseed = report.frame != lastFrame + 1;
            for(usize i = 0; i < NUM_FRAME_SECTION; ++i) {
                const auto raw = report.seconds[i] * MsPerSecond;
                averageMs[i] =
                    reseed ? raw
                           : averageMs[i] + Smoothing * (raw - averageMs[i]);
            }
            lastFrame = report.frame;
        }

        // fed once per new GPU time, not per drawn frame
        if(report.gpu && report.gpu->frame != lastGPUFrame) {
            const auto raw = report.gpu->seconds * MsPerSecond;
            const bool reseed = report.gpu->frame != lastGPUFrame + 1;
            averageGPUMs =
                reseed ? raw : averageGPUMs + Smoothing * (raw - averageGPUMs);
            lastGPUFrame = report.gpu->frame;
        }

        const auto* viewport = ImGui::GetMainViewport();
        const ImVec2 corner{
            viewport->WorkPos.x + viewport->WorkSize.x - Pad,
            viewport->WorkPos.y + Pad
        };
        ImGui::SetNextWindowPos(corner, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("FrameStats", nullptr, Flags);

        if(ImGui::BeginTable("cpu", 2, ImGuiTableFlags_SizingFixedFit)) {
            for(const auto& row: CpuRows) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.label);
                ImGui::TableNextColumn();
                ImGui::Text(
                    "%7.2f ms",
                    averageMs[static_cast<usize>(row.section)]
                );
            }
            // hidden until the device has timed a frame at all
            if(report.gpu) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("GPU");
                ImGui::TableNextColumn();
                ImGui::Text("%7.2f ms", averageGPUMs);
            }
            ImGui::EndTable();
        }

        ImGui::Separator();

        const auto& rhi = report.rhi;
        if(ImGui::BeginTable("counts", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Draws");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%u direct, %u indirect in %u batches",
                rhi.drawCount,
                rhi.indirectDrawCount,
                rhi.indirectBatchCount
            );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Primitives");
            ImGui::TableNextColumn();
            ImGui::Text("%u / %zu", stats.visiblePrimitives, stats.primitives);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Triangles");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%llu",
                static_cast<unsigned long long>(stats.triangles)
            );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Runs");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%zu over %zu pipelines",
                stats.runs,
                stats.pipelines
            );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Barrier edges");
            ImGui::TableNextColumn();
            ImGui::Text("%u", rhi.barrierEdgeCount);

            ImGui::EndTable();
        }

        ImGui::End();
    }
}
