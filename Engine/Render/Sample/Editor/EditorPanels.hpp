#pragma once

#include <vector>

#include "EditorSession.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    struct HierarchyGroup;

    using HierarchyGroups = std::vector<HierarchyGroup>;

    // the hierarchy's folders, in the order the content lists their first row
    HierarchyGroups groupObjects(EditorObjects objects);
    // the rows whose name or detail holds `filter`, ignoring case; every row
    // for an empty one
    std::vector<usize> filterObjects(EditorObjects objects, StrView filter);
    // Raw ImGui panels over a session; each draws into its own window. The
    // highlight draws on the foreground list, without a depth test.
    void drawEditorToolbar(EditorSession& session);
    void drawSelectionHighlight(const EditorSession& session, const RenderScene& scene);

    struct HierarchyGroup {
        Str name;
        std::vector<usize> objects;
    };

    // what the hierarchy remembers between frames
    class HierarchyPanel {
    private:
        HierarchyGroups groups;
        Str filter;
        std::vector<usize> filtered;
        // the row to bring into view once, after a pick or a port write
        std::optional<usize> reveal;

    public:
        // rebuilds the folders; call when the content reloads
        void Reset(EditorObjects objects);
        void Draw(EditorSession& session);
    };
}
