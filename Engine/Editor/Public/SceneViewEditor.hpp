#pragma once

#include <optional>

#include "CommandPort.hpp"
#include "EditorCamera.hpp"
#include "EulerInspector.hpp"
#include "InputProvider.hpp"
#include "PropertyWalker.hpp"
#include "ViewCamera.hpp"
#include "WorldEditor.hpp"

namespace Crowy
{
    // the Scene View's image, window points
    struct SceneImage {
        Vec2 origin;
        Vec2 size;
    };

    // What a host adds to edit a World through a Scene View: the camera the
    // view looks through, the WorldEditor on its image, the "Scene View"
    // window and the selection section of a panel. The port targets of
    // WorldEditor and `sceneCamera` are exposed while it lives.
    class SceneViewEditor {
    private:
        World& world;
        const RenderScene& scene;
        EditorCamera camera;
        // before the panel built on its overrides
        EulerInspector eulerInspector;
        WorldEditor editor;
        std::optional<SceneImage> image;
        // a hold the mouse took, which only the mouse drags and lets go
        bool mouseHolds = false;
        // after everything its callback reaches
        PortExposures cameraExposure;

    public:
        ~SceneViewEditor() = default;
        CROWY_DECLARE_PINNED(SceneViewEditor)

        // a null port exposes nothing
        SceneViewEditor(
            World& world,
            RenderScene& scene,
            const EditorCut& cut,
            CommandPort* port
        );

        // Esc, the mouse on the image and, while the view runs, the camera's
        // keys; whether the cursor hides for a look
        bool PollInput(const InputProvider& input, bool viewRuns);
        // shows the camera in `view` while the view runs
        void Update(f64 seconds, ViewCamera& view, bool viewRuns);
        // the window around `sceneView` (0: none); its content size, while
        // the window is open
        std::optional<Size2D> DrawWindow(u64 sceneView);
        // the editor's update on this frame's image, and the overlay over
        // it; whether the selection section needs building again
        bool UpdateOverlay();
        // the panel section of the selected actor, none without one;
        // `onChange` runs when its fields write
        std::optional<Widget> SelectionSection(const DirtyCallback& onChange);
        ValueOverrides Overrides() { return eulerInspector.Overrides(); }

        EditorCamera& Camera() noexcept { return camera; }
        WorldEditor& Editor() noexcept { return editor; }
    };
}
