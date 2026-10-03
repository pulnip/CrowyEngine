#include "EditorPanels.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "PropertyWalker.hpp"

namespace Crowy
{
    namespace
    {
        bool containsIgnoringCase(StrView text, StrView part) {
            const auto it = std::ranges::search(text, part, [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });

            return !it.empty() || part.empty();
        }

        // window points of a world point already in clip space
        ImVec2 toScreen(const Vec4& clip, Vec2 viewport) {
            return ImVec2{
                (clip.x / clip.w + 1.0f) * 0.5f * viewport.x,
                (1.0f - clip.y / clip.w) * 0.5f * viewport.y
            };
        }

        // the part of a clip-space segment in front of the near plane (z >= 0)
        bool clipToNear(Vec4& a, Vec4& b) {
            if(a.z < 0.0f && b.z < 0.0f)
                return false;
            if(a.z < 0.0f)
                a = a + (b - a) * (a.z / (a.z - b.z));
            else if(b.z < 0.0f)
                b = b + (a - b) * (b.z / (b.z - a.z));

            return true;
        }

        void drawRow(EditorSession& session, usize index, bool reveal) {
            const auto& object = session.Content().Objects()[index];
            const bool selected = session.Selection() == index;
            const auto label = std::format("{}  ·  {}##{}", object.name, object.detail, index);
            if(ImGui::Selectable(label.c_str(), selected)) {
                session.State().selected = object.name;
                session.Sync();
            }
            if(reveal)
                ImGui::SetScrollHereY(0.5f);
        }
    }

    HierarchyGroups groupObjects(EditorObjects objects) {
        HierarchyGroups groups;
        for(usize i = 0; i < objects.size(); ++i) {
            auto group = std::ranges::find(groups, objects[i].group, &HierarchyGroup::name);
            if(group == groups.end()) {
                groups.push_back(HierarchyGroup{.name = objects[i].group});
                group = groups.end() - 1;
            }
            group->objects.push_back(i);
        }

        return groups;
    }

    std::vector<usize> filterObjects(EditorObjects objects, StrView filter) {
        std::vector<usize> rows;
        for(usize i = 0; i < objects.size(); ++i) {
            if(containsIgnoringCase(objects[i].name, filter) || containsIgnoringCase(objects[i].detail, filter))
                rows.push_back(i);
        }

        return rows;
    }

    void drawEditorToolbar(EditorSession& session) {
        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Stage##EditorToolbar")) {
            const auto& content = session.Content();
            auto& state = session.State();

            if(ImGui::BeginCombo("cut", state.cut.c_str())) {
                for(usize i = 0; i < content.Cuts().size(); ++i) {
                    if(ImGui::Selectable(content.Cuts()[i].name.c_str(), content.Cuts()[i].name == state.cut))
                        session.SelectCut(i);
                }
                ImGui::EndCombo();
            }
            if(ImGui::BeginCombo("key", state.key.c_str())) {
                for(usize i = 0; i < content.Keys().size(); ++i) {
                    if(ImGui::Selectable(content.Keys()[i].c_str(), content.Keys()[i] == state.key))
                        session.SelectKey(i);
                }
                ImGui::EndCombo();
            }
            ImGui::TextWrapped("%s", state.status.c_str());
            ImGui::TextDisabled("1-8 cuts, F1-F4 keys, click to select, Esc clears, P hides");
        }
        ImGui::End();
    }

    void drawSelectionHighlight(const EditorSession& session, const RenderScene& scene) {
        const auto selection = session.Selection();
        if(!selection)
            return;

        const auto& camera = session.Camera();
        const auto viewport = session.Viewport();
        const auto viewProj = camera.ViewProj(viewport.x / viewport.y);
        auto* draw = ImGui::GetForegroundDrawList();
        const auto color = IM_COL32(255, 196, 40, 255);

        if(const auto light = session.Content().LightOf(*selection)) {
            const auto& row = scene.Lights().GetRef(*light);
            const auto clip = viewProj * Vec4{row.position.x, row.position.y, row.position.z, 1.0f};
            if(clip.z >= 0.0f) {
                const auto center = toScreen(clip, viewport);
                draw->AddCircle(center, 10.0f, color, 0, 2.0f);
                draw->AddText(ImVec2(center.x + 12.0f, center.y - 8.0f), color, session.State().selected.c_str());
            }
            return;
        }

        const auto primitive = session.Content().PrimitiveOf(*selection);
        if(!primitive || !scene.Primitives().IsValid(*primitive))
            return;

        const auto& row = scene.Primitives().GetRef(*primitive);
        const auto& local = scene.Meshes().GetRef(row.mesh).localBounds;
        std::array<Vec4, 8> corners;
        for(u32 i = 0; i < corners.size(); ++i) {
            const Vec3 sign{i & 1 ? 1.0f : -1.0f, i & 2 ? 1.0f : -1.0f, i & 4 ? 1.0f : -1.0f};
            const auto p = local.center + sign * local.halfScale;
            corners[i] = viewProj * (row.localToWorld * Vec4{p.x, p.y, p.z, 1.0f});
        }

        // the twelve edges join corners one bit apart
        for(u32 a = 0; a < corners.size(); ++a) {
            for(const u32 bit: {1u, 2u, 4u}) {
                const auto b = a | bit;
                if(b == a)
                    continue;
                auto from = corners[a];
                auto to = corners[b];
                if(clipToNear(from, to))
                    draw->AddLine(toScreen(from, viewport), toScreen(to, viewport), color, 2.0f);
            }
        }
        if(corners[0].z >= 0.0f)
            draw->AddText(toScreen(corners[0], viewport), color, session.State().selected.c_str());
    }

    void InspectorPanel::Draw(EditorSession& session, UIContext& context) {
        // the walker seeds values when it builds: rebuild after a write it
        // did not make
        if(session.TakeInspectorDirty() || !built || shown != session.Selection()) {
            sections.clear();
            for(const auto& section: session.Inspected())
                sections.push_back(buildPropertyTree(section.label.c_str(), section.target, *section.desc, section.apply));
            shown = session.Selection();
            built = true;
        }

        ImGui::SetNextWindowPos(ImVec2(1480.0f, 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(432.0f, 420.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Inspector##EditorInspector")) {
            if(const auto selection = session.Selection()) {
                const auto& object = session.Content().Objects()[*selection];
                ImGui::TextUnformatted(object.name.c_str());
                ImGui::TextDisabled("%s  ·  %s", object.group.c_str(), object.detail.c_str());
                ImGui::Separator();
                for(auto& section: sections)
                    std::visit([&](auto& widget) { widget.submit(context); }, section);
            } else {
                ImGui::TextDisabled("click something, or pick it in the hierarchy");
            }
        }
        ImGui::End();
    }

    void HierarchyPanel::Reset(EditorObjects objects) {
        groups = groupObjects(objects);
        filtered = filterObjects(objects, filter);
    }

    void HierarchyPanel::Draw(EditorSession& session) {
        if(session.TakeSelectionChanged())
            reveal = session.Selection();

        ImGui::SetNextWindowPos(ImVec2(8.0f, 140.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 640.0f), ImGuiCond_FirstUseEver);
        if(!ImGui::Begin("Hierarchy##EditorHierarchy")) {
            ImGui::End();
            return;
        }

        const auto objects = session.Content().Objects();
        if(ImGui::InputTextWithHint("##search", "search names and models", &filter))
            filtered = filterObjects(objects, filter);

        if(!filter.empty()) {
            ImGui::TextDisabled("%zu of %zu", filtered.size(), objects.size());
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(filtered.size()));
            const auto at = reveal ? std::ranges::find(filtered, *reveal) : filtered.end();
            if(at != filtered.end())
                clipper.IncludeItemByIndex(static_cast<int>(at - filtered.begin()));
            while(clipper.Step()) {
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                    drawRow(session, filtered[i], reveal == filtered[i]);
            }
        } else {
            for(const auto& group: groups) {
                const auto at = reveal ? std::ranges::find(group.objects, *reveal) : group.objects.end();
                if(at != group.objects.end())
                    ImGui::SetNextItemOpen(true);
                const auto header = std::format("{} ({})", group.name, group.objects.size());
                if(!ImGui::TreeNode(header.c_str()))
                    continue;

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(group.objects.size()));
                if(at != group.objects.end())
                    clipper.IncludeItemByIndex(static_cast<int>(at - group.objects.begin()));
                while(clipper.Step()) {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                        drawRow(session, group.objects[i], reveal == group.objects[i]);
                }
                ImGui::TreePop();
            }
        }
        reveal.reset();
        ImGui::End();
    }
}
