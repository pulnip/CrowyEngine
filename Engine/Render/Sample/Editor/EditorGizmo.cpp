#include "EditorGizmo.hpp"

#include <cmath>

namespace Crowy
{
    std::optional<GizmoLayout> layoutGizmo(
        const EditorCamera& camera,
        Vec2 viewport,
        Vec3 pivot,
        f32 yawDegrees,
        GizmoParts parts
    ) {
        if(!(viewport.x > 0.0f && viewport.y > 0.0f))
            return std::nullopt;

        const auto rotation = camera.Rotation();
        const auto ahead = forward(rotation);
        const auto& lens = camera.lens;
        // depth, not distance, so the size holds off-centre too
        const auto depth = dot(pivot - camera.position, ahead);
        const auto length = GizmoScreenShare
            * (lens.orthographic ? lens.orthoHalfHeight : depth * std::tan(0.5f * lens.fovY));
        // the ring reaches nearest the eye: past the near plane, nothing needs clipping
        if(!(length > 0.0f && depth - GizmoRingRadius * length > lens.nearZ))
            return std::nullopt;

        GizmoLayout layout{
            .pivot = pivot,
            .length = length,
            .parts = parts,
            .viewProj = camera.ViewProj(viewport.x / viewport.y),
            .viewport = viewport
        };
        const auto yaw = static_cast<f32>(toRadian(yawDegrees));
        layout.axes = {
            Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)},
            unitY(),
            Vec3{std::sin(yaw), 0.0f, std::cos(yaw)}
        };
        layout.view = lens.orthographic ? ahead : normalize(pivot - camera.position);

        // straight down or up the level cross vanishes; the screen's left stands in
        const auto across = cross(layout.view, unitY());
        layout.side = normSquared(across) > 1e-6f ? normalize(across) : -right(rotation);
        const auto upRight = right(rotation) + up(rotation);
        layout.diagonal = normalize(upRight - layout.view * dot(upRight, layout.view));
        layout.ringEdgeOn = std::abs(layout.view.y) < GizmoEdgeOn;

        using enum GizmoHandle;
        const auto show = [&](GizmoHandle handle, bool shown) {
            layout.shown[static_cast<usize>(handle)] = shown;
        };
        const auto faces = [&](Vec3 axis) { return std::abs(dot(layout.view, axis)) <= GizmoEndOn; };
        const auto moves = hasFlag(parts, GizmoParts::Move);
        const auto perAxis = hasFlag(parts, GizmoParts::ScaleAxes);
        show(MoveX, moves && faces(unitX()));
        show(MoveY, moves && faces(unitY()));
        show(MoveZ, moves && faces(unitZ()));
        show(Ring, hasFlag(parts, GizmoParts::Turn));
        show(Scale, hasFlag(parts, GizmoParts::Scale));
        show(ScaleX, perAxis && faces(layout.axes[0]));
        show(ScaleY, perAxis && faces(layout.axes[1]));
        show(ScaleZ, perAxis && faces(layout.axes[2]));

        return layout;
    }
}
