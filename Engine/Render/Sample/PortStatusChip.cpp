#include "PortStatusChip.hpp"

#include <imgui.h>

namespace Crowy
{
    void drawPortStatusChip(const CommandPortStatus& status) {
        using enum CommandPortStatus::Server;

        if(!status.everConnected && status.server != BindFailed)
            return;

        // a verb stays lit this many drains after it landed, long enough
        // for a human to see a synchronous one at all
        constexpr u64 HoldFrames = 30;
        constexpr f32 Pad = 10.0f;
        constexpr f32 DotRadius = 4.0f;
        constexpr ImVec4 Gray{0.55f, 0.55f, 0.55f, 1.0f};
        constexpr ImVec4 LightGray{0.75f, 0.75f, 0.75f, 1.0f};
        constexpr ImVec4 White{1.0f, 1.0f, 1.0f, 1.0f};
        constexpr ImVec4 Green{0.35f, 0.85f, 0.40f, 1.0f};
        constexpr ImVec4 Yellow{0.95f, 0.80f, 0.25f, 1.0f};
        constexpr ImVec4 Red{0.90f, 0.30f, 0.30f, 1.0f};
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

        const bool processing =
            status.pendingCount > 0 ||
            (!status.lastVerb.empty() &&
             status.drainCount - status.lastVerbDrain < HoldFrames);
        const ImVec4 dot = status.server == BindFailed ? Red
                           : status.lastReplyFailed    ? Yellow
                           : processing                ? Green
                                                       : Gray;

        const char* text = status.server == BindFailed ? "port failed to bind"
                           : status.lastVerb.empty() ? "no message"
                                                     : status.lastVerb.c_str();

        // sized here rather than auto-resized, which lags a frame: a
        // capture taken on the frame the verb landed must not clip it
        const auto& style = ImGui::GetStyle();
        const auto lineHeight = ImGui::GetTextLineHeight();
        const ImVec2 size{
            style.WindowPadding.x * 2.0f + DotRadius * 2.0f +
                style.ItemSpacing.x + ImGui::CalcTextSize(text).x,
            style.WindowPadding.y * 2.0f + lineHeight
        };
        const auto* viewport = ImGui::GetMainViewport();
        const ImVec2 corner{
            viewport->WorkPos.x + viewport->WorkSize.x - Pad,
            viewport->WorkPos.y + viewport->WorkSize.y - Pad
        };
        ImGui::SetNextWindowPos(corner, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("PortStatus", nullptr, Flags);

        const auto cursor = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(cursor.x + DotRadius, cursor.y + lineHeight * 0.5f),
            DotRadius,
            ImGui::GetColorU32(dot)
        );
        ImGui::Dummy(ImVec2(DotRadius * 2.0f, lineHeight));
        ImGui::SameLine();

        if(!processing || status.server == BindFailed) {
            ImGui::TextColored(Gray, "%s", text);
        } else {
            // one character lit at a time, stepping with the drain
            const auto lit = static_cast<usize>(
                (status.drainCount - status.lastVerbDrain) %
                status.lastVerb.size()
            );
            for(usize i = 0; i < status.lastVerb.size(); ++i) {
                if(i > 0)
                    ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(
                    i == lit ? White : LightGray,
                    "%c",
                    status.lastVerb[i]
                );
            }
        }

        ImGui::End();
    }
}
