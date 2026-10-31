#include "EditorGizmo.hpp"

#include <array>
#include <cmath>

#include "ClassRegistry.hpp"

namespace Crowy
{

    std::optional<GizmoLayout> layoutGizmo(
        const EditorCamera& camera,
        Vec2 viewport,
        Vec3 pivot,
        Vec4 rotation,
        GizmoParts parts
    ) {
        // an unforeshortened arrow spans this share of the viewport's half
        // height
        constexpr auto ScreenShare = 0.15f;
        // |view . axis| above it: the axis points at the eye and its arrow
        // hides
        constexpr auto EndOn = 0.99f;
        // |view . axis| below it: a ring is seen within 10 degrees of edge-on
        constexpr auto EdgeOn = 0.17f;

        if(!(viewport.x > 0.0f && viewport.y > 0.0f))
            return std::nullopt;

        const auto eye = camera.Rotation();
        const auto ahead = forward(eye);
        const auto& lens = camera.lens;
        // depth, not distance, so the size holds off-center too
        const auto depth = dot(pivot - camera.position, ahead);
        const auto length =
            ScreenShare * (lens.orthographic
                               ? lens.orthoHalfHeight
                               : depth * std::tan(0.5f * lens.fovY));
        // the ring reaches nearest the eye: past the near plane, nothing needs
        // clipping
        if(!(length > 0.0f && depth - GizmoRingRadius * length > lens.nearZ))
            return std::nullopt;

        GizmoLayout layout{
            .pivot = pivot,
            .length = length,
            .viewProj = camera.ViewProj(viewport.x / viewport.y),
            .viewport = viewport
        };
        layout.axes = {right(rotation), up(rotation), forward(rotation)};
        layout.view =
            lens.orthographic ? ahead : normalize(pivot - camera.position);

        // looking along a ring's axis its cross vanishes; the screen's left
        // stands in
        const std::array<Vec3, 3> worldAxes{unitX(), unitY(), unitZ()};
        for(usize i = 0; i < worldAxes.size(); ++i) {
            const auto across = cross(layout.view, worldAxes[i]);
            layout.ringSides[i] =
                normSquared(across) > 1e-6f ? normalize(across) : -right(eye);
            layout.ringEdgeOn[i] =
                std::abs(dot(layout.view, worldAxes[i])) < EdgeOn;
        }
        const auto upRight = right(eye) + up(eye);
        layout.diagonal = normalize(projectOntoPlane(upRight, layout.view));

        using enum GizmoHandle;
        const auto show = [&](GizmoHandle handle, bool shown) {
            layout.shown[static_cast<usize>(handle)] = shown;
        };
        const auto faces = [&](Vec3 axis) {
            return std::abs(dot(layout.view, axis)) <= EndOn;
        };
        const auto moves = hasFlag(parts, GizmoParts::Move);
        const auto perAxis = hasFlag(parts, GizmoParts::ScaleAxes);
        show(MoveX, moves && faces(unitX()));
        show(MoveY, moves && faces(unitY()));
        show(MoveZ, moves && faces(unitZ()));
        show(Ring, hasFlag(parts, GizmoParts::Turn));
        show(RingX, hasFlag(parts, GizmoParts::TurnX));
        show(RingZ, hasFlag(parts, GizmoParts::TurnZ));
        show(Scale, hasFlag(parts, GizmoParts::Scale));
        show(ScaleX, perAxis && faces(layout.axes[0]));
        show(ScaleY, perAxis && faces(layout.axes[1]));
        show(ScaleZ, perAxis && faces(layout.axes[2]));

        return layout;
    }

    // clang-format off
    CROWY_STRUCT(GizmoAim)
        .AddProperty("grab", &GizmoAim::grab)
        .AddProperty("reach", &GizmoAim::reach)
    CROWY_STRUCT_END(GizmoAim)
    // clang-format on
}
