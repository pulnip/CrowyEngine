#pragma once

#include "Camera.hpp"
#include "RenderCamera.hpp"

namespace Crowy
{
    // A camera posed by whatever writes its snapshot, a World's camera
    // extraction above all; it takes no input of its own.
    class SnapshotCamera: public Camera {
    private:
        CameraSnapshot snapshot;

    public:
        explicit SnapshotCamera(const CameraSnapshot& snapshot = {}) noexcept
            : snapshot(snapshot) {}

        Vec3 Position() const noexcept override { return snapshot.position; }
        Mat4 View() const noexcept override { return snapshot.view; }
        Mat4 Projection(f32 aspect) const noexcept override {
            return perspective(
                snapshot.fovY,
                aspect,
                snapshot.nearZ,
                snapshot.farZ
            );
        }

        CameraSnapshot& Snapshot() noexcept { return snapshot; }
    };
}
