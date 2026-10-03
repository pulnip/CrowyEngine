#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

#include "EditorCamera.hpp"
#include "EditorPick.hpp"
#include "EnumUtil.hpp"
#include "Geometry/Ray3D.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    struct GizmoAim;
    struct GizmoDrag;
    struct GizmoEdit;
    struct GizmoLayout;
    struct GizmoMark;
    struct GizmoScreen;

    // GizmoHandle's enumerators, None included
    inline constexpr usize GizmoHandleCount = 9;
    inline constexpr usize GizmoRingSamples = 64;
    // in arrow lengths: the ring's radius, and how far out the uniform and
    // the per-axis squares sit
    inline constexpr f32 GizmoRingRadius = 1.25f;
    inline constexpr f32 GizmoScaleOffset = 0.7f;
    inline constexpr f32 GizmoAxisScaleOffset = 0.6f;
    // half a scale square's side, window points
    inline constexpr f32 GizmoSquarePoints = 5.0f;

    // in the order a tie under the cursor goes to: squares, arrows, the ring
    enum class GizmoHandle : u8 {
        None,
        ScaleX,
        ScaleY,
        ScaleZ,
        Scale,
        MoveX,
        MoveY,
        MoveZ,
        Ring,
    };

    // what an object's gizmo offers on its row's "position", "yaw" and "scale"
    enum class GizmoParts : u8 {
        None = 0,
        Move = 1 << 0,
        Turn = 1 << 1,
        Scale = 1 << 2,
        // a square per local axis, for a row scaled per axis
        ScaleAxes = 1 << 3,
    };

    // the nearest multiple of `step`
    inline f32 snapTo(f32 value, f32 step) noexcept {
        return std::round(value / step) * step;
    }

    // into [0, 360); a hair under 360 lands on 0, never on 360
    inline f32 wrapDegrees(f32 degrees) noexcept {
        const auto wrapped = degrees - 360.0f * std::floor(degrees / 360.0f);

        return wrapped >= 360.0f ? 0.0f : wrapped;
    }

    inline constexpr bool isGizmoArrow(GizmoHandle handle) noexcept {
        return handle == GizmoHandle::MoveX || handle == GizmoHandle::MoveY || handle == GizmoHandle::MoveZ;
    }

    inline constexpr bool isGizmoSquare(GizmoHandle handle) noexcept {
        return handle != GizmoHandle::None && handle != GizmoHandle::Ring && !isGizmoArrow(handle);
    }

    // window points from `p` to the segment a-b
    inline f32 distanceToSegment(Vec2 p, Vec2 a, Vec2 b) noexcept {
        const auto along = b - a;
        const auto length = dot(along, along);
        const auto t = length > 0.0f ? std::clamp(dot(p - a, along) / length, 0.0f, 1.0f) : 0.0f;
        const auto offset = p - (a + along * t);

        return std::sqrt(dot(offset, offset));
    }

    // the gizmo of a row at `pivot` seen through `camera`; nothing when the
    // pivot is behind the eye or the ring would reach the near plane
    std::optional<GizmoLayout> layoutGizmo(
        const EditorCamera& camera,
        Vec2 viewport,
        Vec3 pivot,
        f32 yawDegrees,
        GizmoParts parts
    );
    // an arrow's or a square's way from the pivot; zero for the ring
    inline constexpr Vec3 gizmoAxis(const GizmoLayout& layout, GizmoHandle handle) noexcept;
    inline GizmoScreen projectGizmo(const GizmoLayout& layout);
    // the shown square or arrow tip nearest `pixel` within 6 points, else
    // the nearest shaft or ring; None past them all
    inline GizmoHandle hoverGizmo(const GizmoScreen& screen, Vec2 pixel);
    // where a press takes the handle, and where dragging it makes one unit:
    // 1 m, +90 degrees, x2; nothing for a hidden handle
    inline constexpr std::optional<GizmoAim> aimGizmo(const GizmoLayout& layout, GizmoHandle handle);
    // the handle held from a press along `press`; nothing when it misses the
    // handle's plane or lands where the handle cannot measure from
    inline std::optional<GizmoDrag> grabGizmo(const GizmoLayout& layout, GizmoHandle handle, const Ray3D& press);
    // where the cursor's ray meets the held handle's plane, near enough to mean it
    inline std::optional<Vec3> meetGizmo(const GizmoDrag& drag, const Ray3D& ray);
    // the change since the grab with the cursor's ray at `ray`; nothing when
    // the ray misses the plane, so the last change stands
    inline std::optional<GizmoEdit> dragGizmo(const GizmoDrag& drag, const Ray3D& ray, bool snap);

    struct GizmoLayout {
        Vec3 pivot{};
        // meters an unforeshortened arrow spans; the rest are multiples of it
        f32 length = 0.0f;
        // by GizmoHandle; an arrow seen end-on is not shown
        std::array<bool, GizmoHandleCount> shown{};
        // unit: the row's local axes turned by its yaw
        std::array<Vec3, 3> axes{};
        // unit, from the eye to the pivot; the lens's forward when orthographic
        Vec3 view{};
        // unit and level, across the view; the ring is grabbed at pivot - side
        Vec3 side{};
        // unit, up and right on screen, in the plane facing the view
        Vec3 diagonal{};
        // the ring turns by the drag's length rather than its angle
        bool ringEdgeOn = false;
        Mat4 viewProj = unitMat();
        Vec2 viewport{1.0f, 1.0f};
    };

    // an arrow's grabbable stretch, or a square's center at both ends
    struct GizmoMark {
        bool shown = false;
        Vec2 from{};
        Vec2 to{};
    };

    struct GizmoScreen {
        Vec2 pivot{};
        // by GizmoHandle
        std::array<GizmoMark, GizmoHandleCount> marks{};
        // closed, while marks[Ring] is shown
        std::array<Vec2, GizmoRingSamples> ring{};
    };

    // window points
    struct GizmoAim {
        Vec2 grab = EditorNoPick;
        Vec2 reach = EditorNoPick;
    };

    // a held handle: its plane and where the press met it, fixed until release
    struct GizmoDrag {
        GizmoHandle handle = GizmoHandle::None;
        Vec3 pivot{};
        // unit: the way an arrow moves or a square measures
        Vec3 axis{};
        // unit: the plane the cursor's rays meet
        Vec3 normal{};
        Vec3 pressHit{};
        f32 radius = 0.0f;
        Vec3 side{};
        bool edgeOn = false;
        // meters: the farthest from the pivot a hit still means anything
        f32 reach = 0.0f;
    };

    // what a drag makes of the row as the grab found it
    struct GizmoEdit {
        // meters, added to the grabbed position
        Vec3 offset{};
        // degrees, added to the grabbed yaw
        f32 turn = 0.0f;
        // multiplies the grabbed scale per local axis
        Vec3 factor{1.0f, 1.0f, 1.0f};
    };

    inline constexpr Vec3 gizmoAxis(const GizmoLayout& layout, GizmoHandle handle) noexcept {
        using enum GizmoHandle;

        // a level square sits on whichever side of its axis points away from
        // the +X and +Z arrows, so no yaw lays it on a shaft
        const auto away = [](Vec3 axis) { return dot(axis, unitX() + unitZ()) >= 0.0f ? -axis : axis; };
        switch(handle) {
        case MoveX:
            return unitX();
        case MoveY:
            return unitY();
        case MoveZ:
            return unitZ();
        case ScaleX:
            return away(layout.axes[0]);
        case ScaleY:
            return -layout.axes[1];
        case ScaleZ:
            return away(layout.axes[2]);
        case Scale:
            return layout.diagonal;
        default:
            return Vec3{};
        }
    }

    inline GizmoScreen projectGizmo(const GizmoLayout& layout) {
        const auto at = [&](Vec3 point) {
            return projectToWindow(layout.viewProj, point, layout.viewport).value_or(EditorNoPick);
        };
        const auto length = layout.length;

        GizmoScreen screen{.pivot = at(layout.pivot)};
        for(usize i = 1; i < GizmoHandleCount; ++i) {
            const auto handle = static_cast<GizmoHandle>(i);
            auto& mark = screen.marks[i];
            mark.shown = layout.shown[i];
            if(!mark.shown || handle == GizmoHandle::Ring)
                continue;

            const auto axis = gizmoAxis(layout, handle);
            if(isGizmoArrow(handle)) {
                // short of the pivot, so a click there still picks
                constexpr auto ShaftStart = 0.2f;
                mark.from = at(layout.pivot + axis * (ShaftStart * length));
                mark.to = at(layout.pivot + axis * length);
            } else {
                const auto offset = handle == GizmoHandle::Scale ? GizmoScaleOffset : GizmoAxisScaleOffset;
                mark.from = at(layout.pivot + axis * (offset * length));
                mark.to = mark.from;
            }
        }
        if(layout.shown[static_cast<usize>(GizmoHandle::Ring)]) {
            const auto radius = GizmoRingRadius * length;
            for(usize i = 0; i < GizmoRingSamples; ++i) {
                const auto angle = 2.0f * std::numbers::pi_v<f32> * static_cast<f32>(i) / static_cast<f32>(GizmoRingSamples);
                screen.ring[i] = at(layout.pivot + Vec3{std::sin(angle), 0.0f, std::cos(angle)} * radius);
            }
        }

        return screen;
    }

    inline GizmoHandle hoverGizmo(const GizmoScreen& screen, Vec2 pixel) {
        // a handle this many window points from the cursor, or nearer, is under it
        constexpr auto HoverPoints = 6.0f;

        // squares and arrow tips first: overlapping arrows are told apart by their heads
        const auto nearestOf = [&](bool heads) {
            auto best = GizmoHandle::None;
            auto nearest = HoverPoints;
            for(usize i = 1; i < GizmoHandleCount; ++i) {
                const auto handle = static_cast<GizmoHandle>(i);
                const auto& mark = screen.marks[i];
                if(!mark.shown || (handle == GizmoHandle::Ring && heads))
                    continue;

                auto distance = 0.0f;
                if(handle == GizmoHandle::Ring) {
                    distance = 3.0e38f;
                    for(usize j = 0; j < GizmoRingSamples; ++j) {
                        const auto next = screen.ring[(j + 1) % GizmoRingSamples];
                        distance = std::min(distance, distanceToSegment(pixel, screen.ring[j], next));
                    }
                } else if(isGizmoArrow(handle)) {
                    distance = heads ? norm(pixel - mark.to) : distanceToSegment(pixel, mark.from, mark.to);
                } else if(heads) {
                    const auto offset = pixel - mark.from;
                    distance = std::max(0.0f, std::max(std::abs(offset.x), std::abs(offset.y)) - GizmoSquarePoints);
                } else {
                    continue;
                }
                // a tie keeps the earlier handle
                if(distance <= HoverPoints && (best == GizmoHandle::None || distance < nearest)) {
                    best = handle;
                    nearest = distance;
                }
            }

            return best;
        };

        const auto head = nearestOf(true);

        return head != GizmoHandle::None ? head : nearestOf(false);
    }

    inline constexpr std::optional<GizmoAim> aimGizmo(const GizmoLayout& layout, GizmoHandle handle) {
        if(handle == GizmoHandle::None || !layout.shown[static_cast<usize>(handle)])
            return std::nullopt;

        const auto at = [&](Vec3 point) {
            return projectToWindow(layout.viewProj, point, layout.viewport).value_or(EditorNoPick);
        };
        const auto& pivot = layout.pivot;
        const auto length = layout.length;
        const auto axis = gizmoAxis(layout, handle);

        if(isGizmoArrow(handle))
            return GizmoAim{.grab = at(pivot + axis * length), .reach = at(pivot + axis * (length + 1.0f))};
        if(handle == GizmoHandle::Ring) {
            const auto radius = GizmoRingRadius * length;
            const auto start = -layout.side;
            // the start turned +90 degrees: +Z goes to +X, +X to -Z
            const Vec3 turned{start.z, 0.0f, -start.x};
            const auto reach = layout.ringEdgeOn ? pivot : pivot + turned * radius;

            return GizmoAim{.grab = at(pivot + start * radius), .reach = at(reach)};
        }

        const auto offset = (handle == GizmoHandle::Scale ? GizmoScaleOffset : GizmoAxisScaleOffset) * length;

        return GizmoAim{.grab = at(pivot + axis * offset), .reach = at(pivot + axis * (2.0f * offset))};
    }

    inline std::optional<Vec3> meetGizmo(const GizmoDrag& drag, const Ray3D& ray) {
        const auto t = intersectRayPlane(ray, drag.pivot, drag.normal);
        if(!t)
            return std::nullopt;

        const auto hit = ray.origin + ray.direction * *t;
        if(!(norm(hit - drag.pivot) <= drag.reach))
            return std::nullopt;

        return hit;
    }

    inline std::optional<GizmoDrag> grabGizmo(const GizmoLayout& layout, GizmoHandle handle, const Ray3D& press) {
        if(handle == GizmoHandle::None || !layout.shown[static_cast<usize>(handle)])
            return std::nullopt;

        const auto& view = layout.view;
        const auto length = layout.length;
        GizmoDrag drag{
            .handle = handle,
            .pivot = layout.pivot,
            .axis = gizmoAxis(layout, handle),
            .radius = GizmoRingRadius * length,
            .side = layout.side,
            .edgeOn = layout.ringEdgeOn,
            .reach = 1000.0f * length
        };
        // an axis's plane contains it and faces the view most
        if(handle == GizmoHandle::Ring)
            drag.normal = layout.ringEdgeOn ? view : unitY();
        else if(handle == GizmoHandle::Scale)
            drag.normal = view;
        else
            drag.normal = normalize(view - drag.axis * dot(view, drag.axis));

        const auto hit = meetGizmo(drag, press);
        if(!hit)
            return std::nullopt;
        drag.pressHit = *hit;

        const auto out = *hit - layout.pivot;
        constexpr auto Least = 1e-4f;
        if(handle == GizmoHandle::Scale) {
            // the uniform square measures along wherever it was pressed
            if(!(norm(out) > Least * length))
                return std::nullopt;
            drag.axis = normalize(out);
        } else if(handle == GizmoHandle::Ring && !layout.ringEdgeOn) {
            if(!(norm(Vec3{out.x, 0.0f, out.z}) > Least * drag.radius))
                return std::nullopt;
        } else if(isGizmoSquare(handle) && !(dot(out, drag.axis) > Least * length)) {
            return std::nullopt;
        }

        return drag;
    }

    inline std::optional<GizmoEdit> dragGizmo(const GizmoDrag& drag, const Ray3D& ray, bool snap) {
        // Ctrl's steps: meters on an arrow, degrees on the ring, a scale factor
        constexpr auto SnapMeters = 0.25f;
        constexpr auto SnapDegrees = 15.0f;
        constexpr auto SnapFactor = 0.1f;

        const auto hit = meetGizmo(drag, ray);
        if(!hit)
            return std::nullopt;

        GizmoEdit edit;
        if(isGizmoArrow(drag.handle)) {
            auto meters = dot(*hit - drag.pressHit, drag.axis);
            if(snap)
                meters = snapTo(meters, SnapMeters);
            edit.offset = drag.axis * meters;
        } else if(drag.handle == GizmoHandle::Ring) {
            auto turn = 0.0f;
            if(drag.edgeOn) {
                turn = 90.0f * dot(*hit - drag.pressHit, drag.side) / drag.radius;
            } else {
                const auto from = drag.pressHit - drag.pivot;
                const auto to = *hit - drag.pivot;
                if(!(norm(Vec3{to.x, 0.0f, to.z}) > 1e-4f * drag.radius))
                    return std::nullopt;
                // clockwise seen from above: +Z to +X is +90
                turn = static_cast<f32>(toDegree(std::atan2(from.z * to.x - from.x * to.z, from.x * to.x + from.z * to.z)));
            }
            edit.turn = snap ? snapTo(turn, SnapDegrees) : turn;
        } else {
            const auto pressed = dot(drag.pressHit - drag.pivot, drag.axis);
            auto factor = dot(*hit - drag.pivot, drag.axis) / pressed;
            // never through zero: a mirrored row would turn its fronts away
            factor = snap ? std::max(snapTo(factor, SnapFactor), SnapFactor) : std::max(factor, 0.01f);
            using enum GizmoHandle;
            edit.factor = drag.handle == ScaleX ? Vec3{factor, 1.0f, 1.0f}
                        : drag.handle == ScaleY ? Vec3{1.0f, factor, 1.0f}
                        : drag.handle == ScaleZ ? Vec3{1.0f, 1.0f, factor}
                                                : Vec3{factor, factor, factor};
        }

        return edit;
    }

    CROWY_ENUM_BEGIN(GizmoHandle)
        CROWY_ENUM_VALUE(None)
        CROWY_ENUM_VALUE(ScaleX)
        CROWY_ENUM_VALUE(ScaleY)
        CROWY_ENUM_VALUE(ScaleZ)
        CROWY_ENUM_VALUE(Scale)
        CROWY_ENUM_VALUE(MoveX)
        CROWY_ENUM_VALUE(MoveY)
        CROWY_ENUM_VALUE(MoveZ)
        CROWY_ENUM_VALUE(Ring)
    CROWY_ENUM_END()
}
