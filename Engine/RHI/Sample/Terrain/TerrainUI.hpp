#pragma once

#include <vector>

#include "Terrain.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct TerrainDebug {
        bool showNormals = false;
        bool wireframe = false;
    };

    struct TerrainStats {
        u32 vertexCount = 0;
        u32 triangleCount = 0;
        f64 frameTimeMs = 0.0;
    };

    struct UIContext {
        TerrainParams params;
        TerrainDebug debug;
        TerrainStats stats;

        bool autoRebuild = false;
        bool paramsDirty = false;
        bool rebuildRequested = false;
        // a parameter changed outside its widget (Randomize, clamp) - the
        // panel must be rebuilt for the controls to show it
        bool panelDirty = false;

        bool ShouldRebuild() const noexcept {
            return rebuildRequested || (autoRebuild && paramsDirty);
        }
        void ClearRebuild() noexcept {
            rebuildRequested = false;
            paramsDirty = false;
        }
    };

    // a single Column, since UIRenderer::Prepare renders the whole widget tree
    // into one window
    class TerrainPanel {
    private:
        // kept for the rebuild
        std::vector<Widget> sampleStats;
        Widget widget;

    public:
        // the getters in `sampleStats` outlive this call, so they must capture
        // the UIContext this panel is driven with
        TerrainPanel(UIContext&, std::vector<Widget>&& sampleStats = {});

        Widget& Get(UIContext&);
    };
}
