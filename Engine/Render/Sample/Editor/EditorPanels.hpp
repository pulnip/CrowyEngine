#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include "EditorSession.hpp"
#include "RenderScene.hpp"
#include "StringUtil.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct HierarchyGroup;

    using CaptureRequest = std::function<void()>;
    using HierarchyGroups = std::vector<HierarchyGroup>;

    inline constexpr std::vector<usize> filterObjects(
        EditorObjects objects,
        StrView filter
    ) {
        std::vector<usize> rows;
        for(usize i = 0; i < objects.size(); ++i) {
            if(containsIgnoringCase(objects[i].name, filter) ||
               containsIgnoringCase(objects[i].detail, filter))
                rows.push_back(i);
        }

        return rows;
    }

    inline constexpr HierarchyGroups groupObjects(EditorObjects objects);

    // `hint` names the host's keys
    void drawEditorToolbar(
        EditorSession& session,
        StrView hint,
        const CaptureRequest& capture
    );
    void drawLightMarkers(
        const EditorSession& session,
        const RenderScene& scene
    );
    void drawSelectionHighlight(
        const EditorSession& session,
        const RenderScene& scene
    );
    void drawGizmo(const EditorSession& session);

    struct HierarchyGroup {
        Str name;
        std::vector<usize> objects;
    };

    inline constexpr HierarchyGroups groupObjects(EditorObjects objects) {
        HierarchyGroups groups;
        for(usize i = 0; i < objects.size(); ++i) {
            auto group = std::ranges::find(
                groups,
                objects[i].group,
                &HierarchyGroup::name
            );
            if(group == groups.end()) {
                groups.push_back(HierarchyGroup{.name = objects[i].group});
                group = groups.end() - 1;
            }
            group->objects.push_back(i);
        }

        return groups;
    }

    class InspectorPanel {
    private:
        std::vector<Widget> sections;
        std::optional<usize> shown;
        // 0 is never a revision
        u32 revision = 0;

    public:
        void Draw(EditorSession& session, UIContext& context);
    };

    class HierarchyPanel {
    private:
        HierarchyGroups groups;
        Str filter;
        std::vector<usize> filtered;
        // the row to bring into view once, after a pick or a port write
        std::optional<usize> reveal;
        // 0 is never a revision
        u32 revision = 0;

    public:
        void Draw(EditorSession& session);

    private:
        void reset(EditorObjects objects);
    };
}
