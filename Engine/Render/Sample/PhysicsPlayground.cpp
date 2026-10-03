#include <array>
#include <format>
#include <memory>
#include <span>
#include <vector>

#include <imgui.h>

#include "BodyPrimitiveSync.hpp"
#include "FixedTickClock.hpp"
#include "FlyCamera.hpp"
#include "Log.hpp"
#include "MeshGenerator.hpp"
#include "Object.hpp"
#include "PhysicsRuntime.hpp"
#include "PlaygroundScene.hpp"
#include "Puzzle.hpp"
#include "PuzzlePins.hpp"
#include "PuzzleSession.hpp"

namespace Crowy
{
    // exposed as `control.<Kind>`; writing `mode` restarts that puzzle,
    // even in the mode it already runs
    struct PuzzleControl {
        PuzzleMode mode = PuzzleMode::Solution;
    };

    // exposed as `status.<Kind>`, refreshed every frame
    struct PuzzleStatusMirror {
        PuzzleMode mode = PuzzleMode::Solution;
        u64 tick = 0;
        // as the pin table spells it: the port's JSON would round a u64
        Str hash;
        bool solved = false;
        u64 solvedAt = 0;
        u32 checkpointsMatched = 0;
        u32 checkpointsMissed = 0;
    };

    namespace
    {
        PuzzleMode otherMode(PuzzleMode mode) {
            return mode == PuzzleMode::Solution ? PuzzleMode::Control
                                                : PuzzleMode::Solution;
        }

        // a floor level with the arena's is the arena's to draw
        bool isDrawn(const PuzzleBody& body) {
            const auto* box = std::get_if<BoxShape>(&body.desc.shape);
            return body.role != PieceRole::Ground || box == nullptr ||
                   body.desc.pose.position.y + box->halfExtent.y > 0.0f;
        }

        MeshHandle addMeshResource(
            RenderScene& scene,
            const GeometryAllocation& geometry,
            MaterialHandle material,
            const AABB3D& bounds
        ) {
            return scene.Meshes().Add(
                MeshResource{
                    .subMeshes =
                        {SubMesh{.geometry = geometry, .localBounds = bounds}},
                    .materials = {material},
                    .localBounds = bounds
                }
            );
        }

        // a static mark on the floor, a unit quad stretched in x and z
        void addFloorMark(
            RenderScene& scene,
            MeshHandle quad,
            Vec3 center,
            Vec2 halfExtent
        ) {
            const auto localToWorld = translateMat(center) *
                scaleMat({2.0f * halfExtent.x, 1.0f, 2.0f * halfExtent.y});
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = localToWorld,
                    .worldBounds = transformAABB3D(
                        localToWorld,
                        scene.Meshes().GetRef(quad).localBounds
                    ),
                    .mesh = quad
                }
            );
        }

        PuzzleStatusMirror mirrorOf(const PuzzleSession& session) {
            const auto& status = session.GetStatus();

            return PuzzleStatusMirror{
                .mode = session.GetMode(),
                .tick = status.tick,
                .hash = formatHash(status.hash),
                .solved = status.solved,
                .solvedAt = status.solvedAt,
                .checkpointsMatched = status.checkpointsMatched,
                .checkpointsMissed = status.checkpointsMissed,
            };
        }

        // One puzzle on the floor: its session, the glue that draws it, and
        // what the port sees of it.
        struct PuzzleLane {
            PuzzleKind kind = PuzzleKind::PlateGate;
            Vec3 origin = zeros();
            std::unique_ptr<PuzzleSession> session;
            BodyPrimitiveSync sync{zeros()};
            // by body index; invalid for a body the arena draws
            std::vector<PrimitiveHandle> primitives;
            std::vector<Vec3> meshScales;
            MaterialHandle goalMaterial;
            u64 restTicks = 0;
            PuzzleControl control;
            PuzzleStatusMirror status;
        };

        // the materials the puzzles add, by what a body is for
        struct PuzzleMaterials {
            std::array<MaterialHandle, 7> roles{};
            MaterialHandle tile;
            MaterialHandle plate;
            std::array<MaterialHandle, 3> goals{};
        };

        // PlaygroundScene's arena with the puzzles running in front of its
        // stations at real time, drawn one way through BodyPrimitiveSync.
        class PhysicsPlayground final: public PlaygroundScene {
        private:
            // before the lanes, whose worlds borrow it
            PhysicsRuntime runtime;
            // in PuzzleKind order, on the open floor in front of the stations
            std::array<PuzzleLane, 3> lanes{
                PuzzleLane{
                    .kind = PuzzleKind::PlateGate,
                    .origin = {-3.5f, 0.0f, 1.0f},
                },
                PuzzleLane{
                    .kind = PuzzleKind::ToppleBridge,
                    .origin = {0.0f, 0.0f, 1.0f},
                },
                PuzzleLane{
                    .kind = PuzzleKind::SwingDoor,
                    .origin = {3.5f, 0.0f, 1.0f},
                },
            };
            FixedTickClock clock;
            PuzzleMaterials puzzleMaterials;
            GeometryAllocation box{};
            GeometryAllocation ball{};
            GeometryAllocation quad{};
            bool showLabels = true;

        public:
            PhysicsPlayground()
                : PlaygroundScene(makeConfig()) {
                frameThePuzzles();
            }
            ~PhysicsPlayground() override {
                if(auto* port = Port()) {
                    for(const auto& lane: lanes) {
                        const auto name = enumName(lane.kind);
                        port->Unexpose(std::format("control.{}", name));
                        port->Unexpose(std::format("status.{}", name));
                    }
                }
            }
            CROWY_DECLARE_PINNED(PhysicsPlayground)

        protected:
            void OnBuildGeometry(GeometryPool& pool) override {
                PlaygroundScene::OnBuildGeometry(pool);

                const auto boxMesh = MakeBox(0.5f);
                const auto ballMesh = MakeSphere(0.5f, 32, 16);
                const auto quadMesh = MakePlane(Vec2{0.5f, 0.5f});
                box = pool.Add(boxMesh.vertices, boxMesh.indices);
                ball = pool.Add(ballMesh.vertices, ballMesh.indices);
                quad = pool.Add(quadMesh.vertices, quadMesh.indices);
            }

            // the only point where rows may be added: after the base's, before
            // any address is taken
            void OnRestyleMaterials(
                RenderScene& scene,
                const PlaygroundMaterials& materials,
                PlaygroundShading&
            ) override {
                using enum PieceRole;

                // opaque, back faces culled, the default PBR; copied before the
                // first Add moves the rows
                const auto pipeline =
                    scene.Materials().GetRef(materials.bars[0]).pipeline;
                auto add = [&](Vec3 albedo, f32 roughness) {
                    return scene.Materials().Add(
                        MaterialResource{
                            .data = MaterialData{
                                .albedo = albedo,
                                .roughness = roughness
                            },
                            .pipeline = pipeline
                        }
                    );
                };

                auto role = [&](PieceRole piece) -> auto& {
                    return puzzleMaterials.roles[static_cast<usize>(piece)];
                };
                role(Ground) = add({0.66f, 0.63f, 0.58f}, 0.8f);
                role(Wall) = add({0.40f, 0.45f, 0.52f}, 0.7f);
                role(Gate) = add({0.90f, 0.38f, 0.12f}, 0.5f);
                role(Crate) = add({0.88f, 0.66f, 0.18f}, 0.6f);
                role(Slab) = add({0.55f, 0.36f, 0.20f}, 0.7f);
                role(Door) = add({0.10f, 0.58f, 0.58f}, 0.5f);
                role(Ball) = add({0.95f, 0.95f, 0.92f}, 0.3f);
                puzzleMaterials.tile = add({0.16f, 0.18f, 0.22f}, 0.9f);
                puzzleMaterials.plate = add({0.22f, 0.38f, 0.85f}, 0.5f);
                for(auto& goal: puzzleMaterials.goals)
                    goal = add({0.15f, 0.55f, 0.22f}, 0.5f);
            }

            void ExtractScene(RenderScene& scene) override {
                PlaygroundScene::ExtractScene(scene);

                const auto tileMesh = addMeshResource(
                    scene,
                    quad,
                    puzzleMaterials.tile,
                    AABB3D{.halfScale = {0.5f, 0.0f, 0.5f}}
                );
                for(usize i = 0; i < lanes.size(); ++i) {
                    auto& lane = lanes[i];
                    lane.goalMaterial = puzzleMaterials.goals[i];
                    lane.session = std::make_unique<PuzzleSession>(
                        runtime,
                        lane.kind,
                        PuzzleMode::Solution
                    );
                    addPuzzle(scene, lane, tileMesh);
                    bindBodies(lane);
                    lane.sync.Sync(lane.session->GetWorld(), scene);
                    lane.status = mirrorOf(*lane.session);
                }

                if(auto* port = Port()) {
                    for(auto& lane: lanes) {
                        port->Expose(
                            std::format("control.{}", enumName(lane.kind)),
                            &lane.control,
                            *GetDesc<PuzzleControl>(),
                            [this, &lane] { restart(lane, lane.control.mode); }
                        );
                        port->Expose(
                            std::format("status.{}", enumName(lane.kind)),
                            &lane.status,
                            *GetDesc<PuzzleStatusMirror>(),
                            [&lane] { lane.status = mirrorOf(*lane.session); }
                        );
                    }
                }
            }

            void OnProcessInput(const InputProvider& input) override {
                constexpr auto RestartSolutionKey = KeyCode::R;
                constexpr auto RestartControlKey = KeyCode::C;
                constexpr auto FrameKey = KeyCode::F;
                constexpr auto LabelKey = KeyCode::L;

                PlaygroundScene::OnProcessInput(input);

                if(input.IsKeyPressed(RestartSolutionKey)) {
                    for(auto& lane: lanes)
                        restart(lane, PuzzleMode::Solution);
                }
                if(input.IsKeyPressed(RestartControlKey)) {
                    for(auto& lane: lanes)
                        restart(lane, PuzzleMode::Control);
                }
                if(input.IsKeyPressed(FrameKey))
                    frameThePuzzles();
                if(input.IsKeyPressed(LabelKey))
                    showLabels = !showLabels;
            }

            void OnUpdateScene(f64 seconds) override {
                // the puzzles hold at tick 0 this many frames, counted, not
                // timed, so the capture at frame 60 is the same on any machine
                constexpr u64 LeadInFrames = 90;
                constexpr Vec3 GoalGlow{0.3f, 1.6f, 0.45f};

                const auto ticks = FrameNumber() > LeadInFrames
                    ? clock.Advance(toNanoseconds(seconds))
                    : 0u;
                for(u32 tick = 0; tick < ticks; ++tick) {
                    for(auto& lane: lanes)
                        tickLane(lane);
                }

                for(auto& lane: lanes) {
                    lane.sync.Sync(lane.session->GetWorld(), Scene());
                    lane.status = mirrorOf(*lane.session);
                    auto& goal = Scene().Materials().GetRef(lane.goalMaterial);
                    goal.data.emissive =
                        lane.session->GetStatus().solved ? GoalGlow : zeros();
                }
            }

            void OnInitUI(
                RHIDevice& device,
                const OverlayFormats& formats
            ) override {
                PlaygroundScene::OnInitUI(device, formats);

                LOG_INFO(
                    "PhysicsPlayground",
                    "R restarts the solutions, C the controls, F frames the "
                    "puzzles, L toggles the labels"
                );
            }

            // the labels go under the panel and the chip, which the base draws
            std::span<const RHITextureBarrier> OnPrepareUI(
                RHICommandList& cmdList
            ) override {
                if(showLabels)
                    drawLabels();

                return PlaygroundScene::OnPrepareUI(cmdList);
            }

        private:
            static Config makeConfig() {
                auto config = MakeConfig();
                config.drawCapacity = 128;

                return config;
            }

            // high enough to see into every corridor, all three in frame
            void frameThePuzzles() {
                constexpr Vec3 PuzzleViewPosition{0.0f, 4.8f, -3.5f};
                constexpr f32 PuzzleViewPitch = 0.785f;

                auto& camera = static_cast<FlyCamera&>(Camera());
                camera.position = PuzzleViewPosition;
                camera.yaw = 0.0f;
                camera.pitch = PuzzleViewPitch;
                camera.RecomputeView();
            }

            void addPuzzle(
                RenderScene& scene,
                PuzzleLane& lane,
                MeshHandle tile
            ) {
                const auto& puzzle = lane.session->GetPuzzle();

                addFloorMark(
                    scene,
                    tile,
                    lane.origin + Vec3{0.0f, 0.004f, 0.0f},
                    {PuzzleHalfWidth, PuzzleHalfWidth}
                );
                for(const auto& zone: puzzle.zones) {
                    const auto material = zone.role == ZoneRole::Plate
                        ? puzzleMaterials.plate
                        : lane.goalMaterial;
                    const auto bottom = zone.center.y - zone.halfExtent.y;
                    addFloorMark(
                        scene,
                        addMeshResource(
                            scene,
                            quad,
                            material,
                            AABB3D{.halfScale = {0.5f, 0.0f, 0.5f}}
                        ),
                        lane.origin +
                            Vec3{zone.center.x, bottom + 0.008f, zone.center.z},
                        {zone.halfExtent.x, zone.halfExtent.z}
                    );
                }

                lane.primitives.assign(puzzle.bodies.size(), PrimitiveHandle{});
                lane.meshScales.assign(puzzle.bodies.size(), ones());
                for(usize i = 0; i < puzzle.bodies.size(); ++i) {
                    const auto& body = puzzle.bodies[i];
                    if(!isDrawn(body))
                        continue;

                    const auto isBall =
                        std::holds_alternative<SphereShape>(body.desc.shape);
                    const auto mesh = addMeshResource(
                        scene,
                        isBall ? ball : box,
                        puzzleMaterials.roles[static_cast<usize>(body.role)],
                        UnitMeshBounds
                    );
                    lane.primitives[i] =
                        scene.Primitives().Add(PrimitiveSnapshot{.mesh = mesh});
                    lane.meshScales[i] = unitMeshScaleOf(body.desc.shape);
                }
            }

            void bindBodies(PuzzleLane& lane) {
                lane.sync = BodyPrimitiveSync(lane.origin);
                for(u32 i = 0; i < lane.primitives.size(); ++i) {
                    if(!lane.primitives[i].IsValid())
                        continue;
                    lane.sync.Bind(
                        BodyBinding{
                            .body = lane.session->HandleOf(i),
                            .primitive = lane.primitives[i],
                            .meshScale = lane.meshScales[i],
                        }
                    );
                }
            }

            // a new session; both modes build the same bodies in the same
            // order, so the primitives stay and only the bindings change
            void restart(PuzzleLane& lane, PuzzleMode mode) {
                lane.session = std::make_unique<PuzzleSession>(
                    runtime,
                    lane.kind,
                    mode
                );
                lane.control.mode = mode;
                lane.restTicks = 0;
                bindBodies(lane);
                lane.sync.Sync(lane.session->GetWorld(), Scene());
                lane.status = mirrorOf(*lane.session);
            }

            void tickLane(PuzzleLane& lane) {
                // a second at rest after the horizon, then the other mode
                constexpr u64 RestTicks = 60;

                if(lane.session->GetStatus().tick < PuzzleHorizon) {
                    lane.session->Tick();
                    return;
                }
                if(++lane.restTicks >= RestTicks)
                    restart(lane, otherMode(lane.session->GetMode()));
            }

            void drawLabels() {
                constexpr std::array<CStr, 3> Captions{
                    "push the crate onto the plate",
                    "topple the slab across the gap",
                    "swing the door open",
                };

                const auto viewProj = Camera().ViewProj(Aspect());
                const auto* viewport = ImGui::GetMainViewport();
                auto* drawList = ImGui::GetBackgroundDrawList();

                for(usize i = 0; i < lanes.size(); ++i) {
                    const auto& lane = lanes[i];
                    const auto anchor = lane.origin + Vec3{0.0f, 0.0f, -1.75f};
                    const auto clip = viewProj * toVec4(anchor, 1.0f);
                    if(clip.w <= 0.0f)
                        continue;

                    const auto x = viewport->Pos.x +
                        (clip.x / clip.w * 0.5f + 0.5f) * viewport->Size.x;
                    const auto y = viewport->Pos.y +
                        (0.5f - clip.y / clip.w * 0.5f) * viewport->Size.y;
                    const auto& status = lane.session->GetStatus();
                    const auto isSolution =
                        lane.session->GetMode() == PuzzleMode::Solution;
                    const auto title = std::format(
                        "{} - {}",
                        enumName(lane.kind),
                        enumName(lane.session->GetMode())
                    );
                    const auto progress = std::format(
                        "tick {}  solved at {}  pins {}/{}",
                        status.tick,
                        status.solvedAt,
                        status.checkpointsMatched,
                        status.checkpointsMatched + status.checkpointsMissed
                    );

                    const auto lineHeight = ImGui::GetTextLineHeight();
                    const auto width = std::max(
                        {ImGui::CalcTextSize(title.c_str()).x,
                         ImGui::CalcTextSize(Captions[i]).x,
                         ImGui::CalcTextSize(progress.c_str()).x}
                    );
                    const ImVec2 corner{x - width * 0.5f, y};
                    const ImVec2 far{
                        corner.x + width + 6.0f,
                        corner.y + 3.0f * lineHeight + 4.0f
                    };
                    drawList->AddRectFilled(
                        {corner.x - 6.0f, corner.y - 4.0f},
                        far,
                        IM_COL32(16, 18, 22, 200),
                        4.0f
                    );
                    drawList->AddText(
                        corner,
                        isSolution ? IM_COL32(120, 220, 140, 255)
                                   : IM_COL32(240, 190, 90, 255),
                        title.c_str()
                    );
                    drawList->AddText(
                        {corner.x, corner.y + lineHeight},
                        IM_COL32(220, 220, 220, 255),
                        Captions[i]
                    );
                    const auto progressColor = status.checkpointsMissed > 0
                        ? IM_COL32(255, 110, 110, 255)
                        : IM_COL32(180, 180, 180, 255);
                    drawList->AddText(
                        {corner.x, corner.y + 2.0f * lineHeight},
                        progressColor,
                        progress.c_str()
                    );
                }
            }
        };
    }

    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(PuzzleControl)
        .SetProperty("mode", &PuzzleControl::mode)
    CROWY_STRUCT_END(PuzzleControl)

    CROWY_STRUCT(PuzzleStatusMirror)
        .SetProperty("mode", &PuzzleStatusMirror::mode)
        .SetProperty("tick", &PuzzleStatusMirror::tick)
        .SetProperty("hash", &PuzzleStatusMirror::hash)
        .SetProperty("solved", &PuzzleStatusMirror::solved)
        .SetProperty("solvedAt", &PuzzleStatusMirror::solvedAt)
        .SetProperty(
            "checkpointsMatched",
            &PuzzleStatusMirror::checkpointsMatched
        )
        .SetProperty(
            "checkpointsMissed",
            &PuzzleStatusMirror::checkpointsMissed
        )
    CROWY_STRUCT_END(PuzzleStatusMirror)
    // clang-format on
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "PhysicsPlayground",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<PhysicsPlayground>(argc, argv, windowConfig);
}
