#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

#include "EditorCamera.hpp"
#include "EditorPick.hpp"
#include "EnumUtil.hpp"
#include "Geometry/GeoUtil.hpp"
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

    inline constexpr usize GizmoRingSamples = 64;
    // in arrow lengths
    inline constexpr f32 GizmoRingRadius = 1.25f;
    inline constexpr f32 GizmoScaleOffset = 0.7f;
    inline constexpr f32 GizmoAxisScaleOffset = 0.6f;
    // half a scale square's side, window points
    inline constexpr f32 GizmoSquarePoints = 5.0f;

    // in the order a tie under the cursor goes to: squares, arrows, the rings;
    // Ring turns about Y
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
        RingX,
        RingZ,
    };

    // clang-format off
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
        CROWY_ENUM_VALUE(RingX)
        CROWY_ENUM_VALUE(RingZ)
    CROWY_ENUM_END()
    // clang-format on

    inline constexpr usize GizmoHandleCount =
        EnumTraits<GizmoHandle>::entries.size();

    // what an object's gizmo offers on its transform; Turn is the Y ring
    enum class GizmoParts : u8 {
        None = 0,
        Move = 1 << 0,
        Turn = 1 << 1,
        Scale = 1 << 2,
        ScaleAxes = 1 << 3,
        TurnX = 1 << 4,
        TurnZ = 1 << 5,
        Turns = Turn | TurnX | TurnZ,
    };

    inline constexpr bool isGizmoArrow(GizmoHandle handle) noexcept {
        return handle == GizmoHandle::MoveX || handle == GizmoHandle::MoveY ||
               handle == GizmoHandle::MoveZ;
    }

    // the world axis a ring turns about
    inline constexpr std::optional<usize> gizmoRingIndex(
        GizmoHandle handle
    ) noexcept {
        using enum GizmoHandle;

        switch(handle) {
        case RingX:
            return 0;
        case Ring:
            return 1;
        case RingZ:
            return 2;
        default:
            return std::nullopt;
        }
    }

    inline constexpr bool isGizmoRing(GizmoHandle handle) noexcept {
        return gizmoRingIndex(handle).has_value();
    }

    // an arrow's or a per-axis square's axis; none for Scale and the rings
    inline constexpr std::optional<usize> gizmoAxisIndex(
        GizmoHandle handle
    ) noexcept {
        using enum GizmoHandle;

        switch(handle) {
        case MoveX:
        case ScaleX:
            return 0;
        case MoveY:
        case ScaleY:
            return 1;
        case MoveZ:
        case ScaleZ:
            return 2;
        default:
            return std::nullopt;
        }
    }

    inline constexpr bool isGizmoSquare(GizmoHandle handle) noexcept {
        return handle != GizmoHandle::None && !isGizmoRing(handle) &&
               !isGizmoArrow(handle);
    }

    // rotation turns the local axes the squares measure along
    std::optional<GizmoLayout> layoutGizmo(
        const EditorCamera& camera,
        Vec2 viewport,
        Vec3 pivot,
        Vec4 rotation,
        GizmoParts parts
    );
    inline Vec3 gizmoRingPoint(usize ring, f32 angle) noexcept;
    inline constexpr Vec3 gizmoAxis(
        const GizmoLayout& layout,
        GizmoHandle handle
    ) noexcept;
    inline GizmoScreen projectGizmo(const GizmoLayout& layout);
    inline GizmoHandle hoverGizmo(const GizmoScreen& screen, Vec2 pixel);
    // reach is where a drag makes one unit: 1 m, +90 degrees, x2
    inline constexpr std::optional<GizmoAim> aimGizmo(
        const GizmoLayout& layout,
        GizmoHandle handle
    );
    inline std::optional<GizmoDrag> grabGizmo(
        const GizmoLayout& layout,
        GizmoHandle handle,
        const Ray3D& press
    );
    inline std::optional<Vec3> meetGizmo(
        const GizmoDrag& drag,
        const Ray3D& ray
    );
    inline std::optional<GizmoEdit> dragGizmo(
        const GizmoDrag& drag,
        const Ray3D& ray,
        bool snap
    );

    struct GizmoLayout {
        Vec3 pivot{};
        // meters an unforeshortened arrow spans; the rest are multiples of it
        f32 length = 0.0f;
        // by GizmoHandle
        std::array<bool, GizmoHandleCount> shown{};
        // unit: the local axes turned by the rotation
        std::array<Vec3, 3> axes{};
        // unit, from the eye to the pivot; the lens's forward when orthographic
        Vec3 view{};
        // by ring, X, Y, Z: unit, in its plane and across the view; a ring is
        // grabbed at pivot - side
        std::array<Vec3, 3> ringSides{};
        // unit, up and right on screen, in the plane facing the view
        Vec3 diagonal{};
        // by ring: it turns by the drag's length rather than its angle
        std::array<bool, 3> ringEdgeOn{};
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
        // by ring, X, Y, Z: closed, while its mark is shown
        std::array<std::array<Vec2, GizmoRingSamples>, 3> rings{};
    };

    // window points
    struct GizmoAim {
        Vec2 grab = EditorNoPick;
        Vec2 reach = EditorNoPick;
    };

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

    struct GizmoEdit {
        // meters, added to the grabbed position
        Vec3 offset{};
        // degrees about the ring's world axis, seen from its + end
        f32 turn = 0.0f;
        // multiplies the grabbed scale per local axis
        Vec3 factor{1.0f, 1.0f, 1.0f};
    };

    inline constexpr Vec3 gizmoAxis(
        const GizmoLayout& layout,
        GizmoHandle handle
    ) noexcept {
        using enum GizmoHandle;

        // a square sits on whichever side of its axis points away from the +
        // arrows, so no turn lays it on a shaft
        const auto away = [](Vec3 axis) {
            return dot(axis, unitX() + unitY() + unitZ()) >= 0.0f ? -axis
                                                                  : axis;
        };
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
            return away(layout.axes[1]);
        case ScaleZ:
            return away(layout.axes[2]);
        case Scale:
            return layout.diagonal;
        case RingX:
            return unitX();
        case Ring:
            return unitY();
        case RingZ:
            return unitZ();
        default:
            return Vec3{};
        }
    }

    // unit: the ring's circle in its plane; the Y ring runs from +Z to +X
    inline Vec3 gizmoRingPoint(usize ring, f32 angle) noexcept {
        const auto s = std::sin(angle);
        const auto c = std::cos(angle);
        switch(ring) {
        case 0:
            return Vec3{0.0f, s, c};
        case 1:
            return Vec3{s, 0.0f, c};
        default:
            return Vec3{c, s, 0.0f};
        }
    }

    inline GizmoScreen projectGizmo(const GizmoLayout& layout) {
        const auto at = [&](Vec3 point) {
            return projectToWindow(layout.viewProj, point, layout.viewport)
                .value_or(EditorNoPick);
        };
        const auto length = layout.length;

        GizmoScreen screen{.pivot = at(layout.pivot)};
        for(usize i = 1; i < GizmoHandleCount; ++i) {
            const auto handle = static_cast<GizmoHandle>(i);
            auto& mark = screen.marks[i];
            mark.shown = layout.shown[i];
            if(!mark.shown || isGizmoRing(handle))
                continue;

            const auto axis = gizmoAxis(layout, handle);
            if(isGizmoArrow(handle)) {
                // short of the pivot, so a click there still picks
                constexpr auto ShaftStart = 0.2f;
                mark.from = at(layout.pivot + axis * (ShaftStart * length));
                mark.to = at(layout.pivot + axis * length);
            }
            else {
                const auto offset = handle == GizmoHandle::Scale
                                        ? GizmoScaleOffset
                                        : GizmoAxisScaleOffset;
                mark.from = at(layout.pivot + axis * (offset * length));
                mark.to = mark.from;
            }
        }
        const auto radius = GizmoRingRadius * length;
        for(const auto handle:
            {GizmoHandle::RingX, GizmoHandle::Ring, GizmoHandle::RingZ}) {
            if(!layout.shown[static_cast<usize>(handle)])
                continue;

            const auto ring = *gizmoRingIndex(handle);
            for(usize i = 0; i < GizmoRingSamples; ++i) {
                const auto angle = 2.0f * std::numbers::pi_v<f32> *
                                   static_cast<f32>(i) /
                                   static_cast<f32>(GizmoRingSamples);
                screen.rings[ring][i] =
                    at(layout.pivot + gizmoRingPoint(ring, angle) * radius);
            }
        }

        return screen;
    }

    inline GizmoHandle hoverGizmo(const GizmoScreen& screen, Vec2 pixel) {
        constexpr auto HoverPoints = 6.0f;

        // squares and arrow tips first: overlapping arrows are told apart by
        // their heads
        const auto nearestOf = [&](bool heads) {
            auto best = GizmoHandle::None;
            auto nearest = HoverPoints;
            for(usize i = 1; i < GizmoHandleCount; ++i) {
                const auto handle = static_cast<GizmoHandle>(i);
                const auto& mark = screen.marks[i];
                const auto ring = gizmoRingIndex(handle);
                if(!mark.shown || (ring && heads))
                    continue;

                auto distance = 0.0f;
                if(ring) {
                    const auto& samples = screen.rings[*ring];
                    distance = 3.0e38f;
                    for(usize j = 0; j < GizmoRingSamples; ++j) {
                        const auto next = samples[(j + 1) % GizmoRingSamples];
                        distance = std::min(
                            distance,
                            distanceToSegment(pixel, samples[j], next)
                        );
                    }
                }
                else if(isGizmoArrow(handle)) {
                    distance =
                        heads ? norm(pixel - mark.to)
                              : distanceToSegment(pixel, mark.from, mark.to);
                }
                else if(heads) {
                    const auto offset = pixel - mark.from;
                    distance = std::max(
                        0.0f,
                        std::max(std::abs(offset.x), std::abs(offset.y)) -
                            GizmoSquarePoints
                    );
                }
                else {
                    continue;
                }
                // a tie keeps the earlier handle
                if(distance <= HoverPoints &&
                   (best == GizmoHandle::None || distance < nearest)) {
                    best = handle;
                    nearest = distance;
                }
            }

            return best;
        };

        const auto head = nearestOf(true);

        return head != GizmoHandle::None ? head : nearestOf(false);
    }

    inline constexpr std::optional<GizmoAim> aimGizmo(
        const GizmoLayout& layout,
        GizmoHandle handle
    ) {
        if(handle == GizmoHandle::None ||
           !layout.shown[static_cast<usize>(handle)])
            return std::nullopt;

        const auto at = [&](Vec3 point) {
            return projectToWindow(layout.viewProj, point, layout.viewport)
                .value_or(EditorNoPick);
        };
        const auto& pivot = layout.pivot;
        const auto length = layout.length;
        const auto axis = gizmoAxis(layout, handle);

        if(isGizmoArrow(handle))
            return GizmoAim{
                .grab = at(pivot + axis * length),
                .reach = at(pivot + axis * (length + 1.0f))
            };
        if(const auto ring = gizmoRingIndex(handle)) {
            const auto radius = GizmoRingRadius * length;
            const auto start = -layout.ringSides[*ring];
            // the start turned +90 degrees: for the Y ring +Z goes to +X
            const auto turned = cross(axis, start);
            const auto reach =
                layout.ringEdgeOn[*ring] ? pivot : pivot + turned * radius;

            return GizmoAim{
                .grab = at(pivot + start * radius),
                .reach = at(reach)
            };
        }

        const auto offset =
            (handle == GizmoHandle::Scale ? GizmoScaleOffset
                                          : GizmoAxisScaleOffset) *
            length;

        return GizmoAim{
            .grab = at(pivot + axis * offset),
            .reach = at(pivot + axis * (2.0f * offset))
        };
    }

    inline std::optional<Vec3> meetGizmo(
        const GizmoDrag& drag,
        const Ray3D& ray
    ) {
        const auto t = intersectRayPlane(ray, drag.pivot, drag.normal);
        if(!t)
            return std::nullopt;

        const auto hit = ray.origin + ray.direction * *t;
        if(!(norm(hit - drag.pivot) <= drag.reach))
            return std::nullopt;

        return hit;
    }

    inline std::optional<GizmoDrag> grabGizmo(
        const GizmoLayout& layout,
        GizmoHandle handle,
        const Ray3D& press
    ) {
        if(handle == GizmoHandle::None ||
           !layout.shown[static_cast<usize>(handle)])
            return std::nullopt;

        const auto& view = layout.view;
        const auto length = layout.length;
        const auto ring = gizmoRingIndex(handle);
        GizmoDrag drag{
            .handle = handle,
            .pivot = layout.pivot,
            .axis = gizmoAxis(layout, handle),
            .radius = GizmoRingRadius * length,
            .side = ring ? layout.ringSides[*ring] : Vec3{},
            .edgeOn = ring && layout.ringEdgeOn[*ring],
            .reach = 1000.0f * length
        };
        // an axis's plane contains it and faces the view most
        if(ring)
            drag.normal = drag.edgeOn ? view : drag.axis;
        else if(handle == GizmoHandle::Scale)
            drag.normal = view;
        else
            drag.normal = normalize(projectOntoPlane(view, drag.axis));

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
        }
        else if(ring && !drag.edgeOn) {
            const auto across = projectOntoPlane(out, drag.axis);
            if(!(norm(across) > Least * drag.radius))
                return std::nullopt;
        }
        else if(
            isGizmoSquare(handle) && !(dot(out, drag.axis) > Least * length)
        ) {
            return std::nullopt;
        }

        return drag;
    }

    inline std::optional<GizmoEdit> dragGizmo(
        const GizmoDrag& drag,
        const Ray3D& ray,
        bool snap
    ) {
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
        }
        else if(isGizmoRing(drag.handle)) {
            auto turn = 0.0f;
            if(drag.edgeOn) {
                turn =
                    90.0f * dot(*hit - drag.pressHit, drag.side) / drag.radius;
            }
            else {
                const auto& axis = drag.axis;
                const auto from =
                    projectOntoPlane(drag.pressHit - drag.pivot, axis);
                const auto to = projectOntoPlane(*hit - drag.pivot, axis);
                if(!(norm(to) > 1e-4f * drag.radius))
                    return std::nullopt;
                // the Y ring seen from above: +Z to +X is +90
                turn = degreesOf(signedAngle(from, to, axis));
            }
            edit.turn = snap ? snapTo(turn, SnapDegrees) : turn;
        }
        else {
            const auto pressed = dot(drag.pressHit - drag.pivot, drag.axis);
            auto factor = dot(*hit - drag.pivot, drag.axis) / pressed;
            // never through zero: a mirrored row would turn its fronts away
            factor = snap ? std::max(snapTo(factor, SnapFactor), SnapFactor)
                          : std::max(factor, 0.01f);
            if(const auto axis = gizmoAxisIndex(drag.handle)) {
                edit.factor = ones();
                edit.factor[*axis] = factor;
            }
            else {
                edit.factor = Vec3{factor, factor, factor};
            }
        }

        return edit;
    }
}
