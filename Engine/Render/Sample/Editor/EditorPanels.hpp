#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include "EditorSession.hpp"
#include "RenderScene.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct HierarchyGroup;

    using CaptureRequest = std::function<void()>;
    using HierarchyGroups = std::vector<HierarchyGroup>;

    // ASCII letters only, which is what names and models are written in
    inline constexpr bool containsIgnoringCase(StrView text, StrView part) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        const auto found = std::ranges::search(text, part, [&](char a, char b) { return lower(a) == lower(b); });

        return !found.empty() || part.empty();
    }

    // the rows whose name or detail holds `filter`, ignoring case; every row
    // for an empty one
    inline constexpr std::vector<usize> filterObjects(EditorObjects objects, StrView filter) {
        std::vector<usize> rows;
        for(usize i = 0; i < objects.size(); ++i) {
            if(containsIgnoringCase(objects[i].name, filter) || containsIgnoringCase(objects[i].detail, filter))
                rows.push_back(i);
        }

        return rows;
    }

    // the hierarchy's folders, in the order the content lists their first row
    inline constexpr HierarchyGroups groupObjects(EditorObjects objects);

    // Raw ImGui panels over a session, each in its own window; `hint` names
    // the host's keys. Markers and the highlight draw on the foreground list.
    void drawEditorToolbar(EditorSession& session, StrView hint, const CaptureRequest& capture);
    void drawLightMarkers(const EditorSession& session, const RenderScene& scene);
    void drawSelectionHighlight(const EditorSession& session, const RenderScene& scene);
    // the selection's handles, the held or hovered one lit, the change beside the cursor
    void drawGizmo(const EditorSession& session);

    struct HierarchyGroup {
        Str name;
        std::vector<usize> objects;
    };

    inline constexpr HierarchyGroups groupObjects(EditorObjects objects) {
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

    // the selection's sections as the reflection walker draws them, rebuilt
    // when the selection changes or the port writes behind its widgets
    class InspectorPanel {
    private:
        std::vector<Widget> sections;
        std::optional<usize> shown;
        // the revision the widgets were built in; 0 is never
        u32 revision = 0;

    public:
        void Draw(EditorSession& session, UIContext& context);
    };

    // what the hierarchy remembers between frames
    class HierarchyPanel {
    private:
        HierarchyGroups groups;
        Str filter;
        std::vector<usize> filtered;
        // the row to bring into view once, after a pick or a port write
        std::optional<usize> reveal;
        // the revision the folders index; 0 is never
        u32 revision = 0;

    public:
        void Draw(EditorSession& session);

    private:
        void reset(EditorObjects objects);
    };
}
