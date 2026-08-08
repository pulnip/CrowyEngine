#pragma once

#include <vector>
#include "Terrain.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct TerrainDebug{
        bool showNormals = false;
        bool wireframe = false;
    };

    // stats both samples report; sample-specific ones are injected into the
    // panel instead - see TerrainPanel
    struct TerrainStats{
        u32 vertexCount = 0;
        u32 triangleCount = 0;
        f64 buildTimeMs = 0.0;
        f64 frameTimeMs = 0.0;
    };

    struct UIContext{
        TerrainParams params;
        TerrainDebug debug;
        TerrainStats stats;

        bool autoRebuild = false;
        // a parameter control moved since the last build
        bool paramsDirty = false;
        // the [Rebuild] button; ignores autoRebuild
        bool rebuildRequested = false;
        // a parameter changed outside its widget (Randomize, clamp) - the
        // panel must be rebuilt for the controls to show it
        bool panelDirty = false;

        bool ShouldRebuild() const noexcept{
            return rebuildRequested || (autoRebuild && paramsDirty);
        }
        void ClearRebuild() noexcept{
            rebuildRequested = false;
            paramsDirty = false;
        }
    };

    // The shared parameter panel: a single Column, since UIRenderer::Prepare
    // renders the whole widget tree into one window.
    class TerrainPanel{
    private:
        // kept for the rebuild
        std::vector<Widget> sampleStats;
        Widget widget;

    public:
        // `sampleStats` is appended under the Stats heading.
        // their getters must capture the UIContext this panel is
        // driven with - they outlive this call.
        TerrainPanel(UIContext&, std::vector<Widget>&& sampleStats = {});

        // pass to UIRenderer::Prepare once per frame
        Widget& Get(UIContext&);
    };
}
