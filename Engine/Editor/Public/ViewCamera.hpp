#pragma once

#include "EditorCamera.hpp"
#include "SnapshotCamera.hpp"

namespace Crowy
{
    // the World's camera, or the editor's while one is shown
    class ViewCamera final: public SnapshotCamera {
    private:
        const EditorCamera* shown = nullptr;

    public:
        void Show(const EditorCamera* camera) noexcept { shown = camera; }

        Vec3 Position() const noexcept override {
            return shown ? shown->Position() : SnapshotCamera::Position();
        }
        Mat4 View() const noexcept override {
            return shown ? shown->View() : SnapshotCamera::View();
        }
        Mat4 Projection(f32 aspect) const noexcept override {
            return shown ? shown->Projection(aspect)
                         : SnapshotCamera::Projection(aspect);
        }
    };
}
