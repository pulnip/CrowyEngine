#include "WorldEditorOverlay.hpp"

#include <algorithm>
#include <array>

#include <imgui.h>

#include "Geometry/Overlap3D.hpp"
#include "Geometry/Projection.hpp"

namespace Crowy
{
    namespace
    {
        constexpr std::array<ImU32, 3> AxisColors{
            IM_COL32(235, 70, 70, 255),
            IM_COL32(90, 205, 90, 255),
            IM_COL32(80, 130, 245, 255),
        };
        constexpr ImU32 HighlightColor = IM_COL32(255, 196, 40, 255);
        constexpr ImU32 MarkerColor = IM_COL32(255, 236, 160, 230);

        struct OverlayView {
            ImDrawList* draw = nullptr;
            Vec2 origin;
            Vec2 size;
            Mat4 viewProj;

            ImVec2 Window(Vec2 point) const noexcept {
                return ImVec2{point.x + origin.x, point.y + origin.y};
            }
        };

        void drawMarkers(
            const OverlayView& view,
            const WorldEditor& editor,
            const World& world
        ) {
            for(const auto actor: editor.MarkedActors()) {
                const auto at = projectToWindow(
                    view.viewProj,
                    world.WorldPosition(actor),
                    view.size
                );
                if(at)
                    view.draw
                        ->AddCircleFilled(view.Window(*at), 4.0f, MarkerColor);
            }
        }

        void drawMeshBox(
            const OverlayView& view,
            const RenderScene& scene,
            PrimitiveHandle primitive,
            CStr name
        ) {
            const auto& row = scene.Primitives().GetRef(primitive);
            const auto& local = scene.Meshes().GetRef(row.mesh).localBounds;
            std::array<Vec4, 8> corners;
            for(u32 i = 0; i < corners.size(); ++i) {
                corners[i] = view.viewProj * (row.localToWorld *
                                              toVec4(cornerOf(local, i), 1.0f));
            }

            const auto screen = [&](const Vec4& clip) {
                return view.Window(clipToWindow(clip, view.size));
            };
            // the twelve edges join corners one bit apart
            for(u32 a = 0; a < corners.size(); ++a) {
                for(const u32 bit: {1u, 2u, 4u}) {
                    const auto b = a | bit;
                    if(b == a)
                        continue;
                    auto from = corners[a];
                    auto to = corners[b];
                    if(clipSegmentToNear(from, to))
                        view.draw->AddLine(
                            screen(from),
                            screen(to),
                            HighlightColor,
                            2.0f
                        );
                }
            }
            if(corners[0].z >= 0.0f)
                view.draw->AddText(screen(corners[0]), HighlightColor, name);
        }

        void drawSelection(
            const OverlayView& view,
            const WorldEditor& editor,
            const World& world,
            const RenderScene& scene
        ) {
            const auto selection = editor.Selection();
            if(!selection)
                return;

            const auto name = editor.DisplayName(*selection);
            if(const auto primitive = primitiveOfActor(world, *selection)) {
                drawMeshBox(view, scene, *primitive, name.c_str());
                return;
            }

            const auto at = projectToWindow(
                view.viewProj,
                world.WorldPosition(*selection),
                view.size
            );
            if(!at)
                return;

            const auto center = view.Window(*at);
            view.draw->AddCircle(center, 10.0f, HighlightColor, 0, 2.0f);
            view.draw->AddText(
                ImVec2(center.x + 12.0f, center.y - 8.0f),
                HighlightColor,
                name.c_str()
            );
        }

        void drawGizmo(const OverlayView& view, const WorldEditor& editor) {
            const auto layout = editor.SelectionGizmo();
            if(!layout)
                return;

            using enum GizmoHandle;
            const auto screen = projectGizmo(*layout);
            const auto& io = ImGui::GetIO();
            const Vec2 mouse{io.MousePos.x, io.MousePos.y};
            const auto held = editor.Held();
            const auto lit = held != None ? held
                             : io.WantCaptureMouse
                                 ? None
                                 : hoverGizmo(screen, mouse - view.origin);
            const auto colorOf = [&](GizmoHandle handle, ImU32 color) {
                return handle == lit ? HighlightColor : color;
            };

            for(const auto handle: {RingX, Ring, RingZ}) {
                if(!screen.marks[static_cast<usize>(handle)].shown)
                    continue;

                const auto index = *gizmoRingIndex(handle);
                std::array<ImVec2, GizmoRingSamples> ring;
                for(usize i = 0; i < ring.size(); ++i)
                    ring[i] = view.Window(screen.rings[index][i]);
                view.draw->AddPolyline(
                    ring.data(),
                    static_cast<int>(ring.size()),
                    colorOf(handle, AxisColors[index]),
                    ImDrawFlags_Closed,
                    2.0f
                );
            }
            for(const auto handle: {MoveX, MoveY, MoveZ}) {
                const auto& mark = screen.marks[static_cast<usize>(handle)];
                if(!mark.shown)
                    continue;

                const auto drawn =
                    colorOf(handle, AxisColors[*gizmoAxisIndex(handle)]);
                const auto along = mark.to - screen.pivot;
                const auto length = norm(along);
                view.draw->AddLine(
                    view.Window(screen.pivot),
                    view.Window(mark.to),
                    drawn,
                    3.0f
                );
                // the head straddles the tip, inside the 6 points a press takes
                if(length > 1.0f) {
                    const auto way = along / length;
                    const Vec2 across{-way.y, way.x};
                    const auto base = mark.to - way * 4.0f;
                    view.draw->AddTriangleFilled(
                        view.Window(mark.to + way * 6.0f),
                        view.Window(base + across * 5.0f),
                        view.Window(base - across * 5.0f),
                        drawn
                    );
                }
            }
            for(const auto handle: {ScaleX, ScaleY, ScaleZ}) {
                const auto& mark = screen.marks[static_cast<usize>(handle)];
                if(!mark.shown)
                    continue;

                const Vec2 half{GizmoSquarePoints, GizmoSquarePoints};
                view.draw->AddRectFilled(
                    view.Window(mark.from - half),
                    view.Window(mark.from + half),
                    colorOf(handle, AxisColors[*gizmoAxisIndex(handle)])
                );
            }
        }
    }

    void drawWorldEditorOverlay(
        const WorldEditor& editor,
        const World& world,
        const RenderScene& scene,
        const EditorCamera& camera,
        Vec2 origin,
        Vec2 size
    ) {
        const OverlayView view{
            .draw = ImGui::GetForegroundDrawList(),
            .origin = origin,
            .size = size,
            .viewProj = camera.ViewProj(size.x / size.y)
        };
        const auto end = origin + size;
        view.draw->PushClipRect(
            ImVec2{origin.x, origin.y},
            ImVec2{end.x, end.y},
            true
        );
        drawMarkers(view, editor, world);
        drawSelection(view, editor, world, scene);
        drawGizmo(view, editor);
        view.draw->PopClipRect();
    }
}
