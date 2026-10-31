#pragma once

#include "WorldEditor.hpp"

namespace Crowy
{
    // what the editor shows over a view whose top-left is `origin` in window
    // points: a marker for each actor with no mesh, the selection, the gizmo
    void drawWorldEditorOverlay(
        const WorldEditor& editor,
        const World& world,
        const RenderScene& scene,
        const EditorCamera& camera,
        Vec2 origin,
        Vec2 size
    );
}
