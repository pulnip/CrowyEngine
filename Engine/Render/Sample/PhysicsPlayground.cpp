#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <imgui.h>

#include "BodyPrimitiveSync.hpp"
#include "FixedTickClock.hpp"
#include "FlyCamera.hpp"
#include "InputProvider.hpp"
#include "Log.hpp"
#include "MeshGenerator.hpp"
#include "Object.hpp"
#include "PhysicsRuntime.hpp"
#include "PortStatusChip.hpp"
#include "Puzzle.hpp"
#include "PuzzlePins.hpp"
#include "PuzzleSession.hpp"
#include "RenderApp.hpp"
#include "StatsOverlay.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    struct UIContext {};

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
        u32 eventsApplied = 0;
    };

    namespace
    {
        using RoleMaterials =
            std::array<MaterialHandle, EnumTraits<PieceRole>::entries.size()>;

        // three lanes a row, 3.5 m wide with a metre between them
        inline constexpr usize MaxLanes = 6;
        inline constexpr f32 LaneSpacing = 4.5f;

        PuzzleMode otherMode(PuzzleMode mode) {
            return mode == PuzzleMode::Solution ? PuzzleMode::Control
                                                : PuzzleMode::Solution;
        }

        Vec3 laneOrigin(usize lane) {
            const auto column = static_cast<f32>(lane % 3);
            const auto row = static_cast<f32>(lane / 3);

            return Vec3{LaneSpacing * (column - 1.0f), 0.0f, LaneSpacing * row};
        }

        // a floor level with the playground's own is the playground's to
        // draw
        bool isDrawn(const PuzzleBody& body) {
            const auto* box = std::get_if<BoxShape>(&body.desc.shape);
            return body.role != PieceRole::Ground || box == nullptr ||
                   body.desc.pose.position.y + box->halfExtent.y > 0.0f;
        }

        MaterialPipelineDesc basePipeline(CStr file, CStr fragmentEntry) {
            return MaterialPipelineDesc{
                .vertexShader = {.path = file, .entryPoint = "vs_main"},
                .fragmentShader = {.path = file, .entryPoint = fragmentEntry},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        MaterialPipelineDesc translucentPipeline(CStr file) {
            auto pipeline = basePipeline(file, "fs_translucent");
            pipeline.domain = MaterialDomain::Translucent;

            RHIBlendState blend{};
            blend.renderTargets[0] = RHIRenderTargetBlendState{
                .blendEnable = true,
                .srcBlend = RHIBlend::SrcAlpha,
                .dstBlend = RHIBlend::InvSrcAlpha
            };
            pipeline.blend = blend;

            return pipeline;
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

        // a static primitive whose bounds come from its matrix
        PrimitiveHandle addStatic(
            RenderScene& scene,
            MeshHandle mesh,
            const Mat4& localToWorld,
            PrimitiveFlags flags
        ) {
            return scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = localToWorld,
                    .worldBounds = transformAABB3D(
                        localToWorld,
                        scene.Meshes().GetRef(mesh).localBounds
                    ),
                    .mesh = mesh,
                    .flags = flags
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
                .eventsApplied = status.eventsApplied,
            };
        }

        // a drawn body, or one part of a drawn compound
        struct LaneBody {
            u32 body = 0;
            PrimitiveHandle primitive;
            std::optional<BodyPose> part;
            Vec3 meshScale = ones();
        };

        struct LaneTether {
            u32 hinge = 0;
            PrimitiveHandle primitive;
        };

        // One puzzle on the floor: its session, the glue that draws it, and
        // what the port sees of it.
        struct PuzzleLane {
            PuzzleKind kind = PuzzleKind::KickerJump;
            Vec3 origin = zeros();
            std::unique_ptr<PuzzleSession> session;
            BodyPrimitiveSync sync{zeros()};
            std::vector<LaneBody> bodies;
            std::vector<LaneTether> tethers;
            MaterialHandle goalMaterial;
            u64 restTicks = 0;
            PuzzleControl control;
            PuzzleStatusMirror status;
        };

        using PuzzleLanes = std::vector<PuzzleLane>;

        struct PuzzleMaterials {
            MaterialHandle floor;
            MaterialHandle tile;
            MaterialHandle tether;
            MaterialHandle water;
            // a Goal body takes its lane's goal material instead
            RoleMaterials roles{};
        };

        // Every puzzle of the library in its own lane at real time, on a bare
        // floor under one sun, drawn one way through BodyPrimitiveSync.
        class PhysicsPlayground final: public RenderApp {
        private:
            static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};
            static constexpr CStr StandardForward =
                "Engine/Render/Shader/StandardForward.slang";
            // every lane in frame, high enough to see into each one
            static constexpr Vec3 PuzzleViewPosition{0.0f, 7.3f, -3.9f};
            static constexpr f32 PuzzleViewPitch = 0.873f;
            // a margin round the lanes, no wider: it sets the shadow fit
            static constexpr Vec3 FloorHalf{10.0f, 0.0f, 8.0f};

            // before the lanes, whose worlds borrow it
            PhysicsRuntime runtime;
            PuzzleLanes lanes;
            FixedTickClock clock;
            PuzzleMaterials materials;
            GeometryAllocation box{};
            GeometryAllocation ball{};
            GeometryAllocation quad{};
            GeometryAllocation floorPlane{};
            RAII<UIRenderer> uiRenderer;
            UIContext uiContext;
            StatsOverlay statsOverlay;
            bool showLabels = true;

        public:
            PhysicsPlayground()
                : RenderApp(
                      makeConfig(),
                      std::make_unique<FlyCamera>(makeCamera())
                  ) {
                constexpr auto& Kinds = EnumTraits<PuzzleKind>::entries;
                static_assert(Kinds.size() <= MaxLanes);

                for(usize i = 0; i < Kinds.size(); ++i) {
                    lanes.push_back(
                        PuzzleLane{
                            .kind = Kinds[i].value,
                            .origin = laneOrigin(i),
                        }
                    );
                }
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
                const auto boxMesh = MakeBox(0.5f);
                const auto ballMesh = MakeSphere(0.5f, 32, 16);
                const auto quadMesh = MakePlane(Vec2{0.5f, 0.5f});
                const auto floorMesh =
                    MakePlane(Vec2{FloorHalf.x, FloorHalf.z});
                box = pool.Add(boxMesh.vertices, boxMesh.indices);
                ball = pool.Add(ballMesh.vertices, ballMesh.indices);
                quad = pool.Add(quadMesh.vertices, quadMesh.indices);
                floorPlane = pool.Add(floorMesh.vertices, floorMesh.indices);
            }

            void ExtractScene(RenderScene& scene) override {
                addLight(scene);
                // every row before any address is taken: the table is a
                // vector, so a later Add could move the rows
                addMaterials(scene);
                addFloor(scene);

                for(auto& lane: lanes) {
                    lane.session = std::make_unique<PuzzleSession>(
                        runtime,
                        lane.kind,
                        PuzzleMode::Solution
                    );
                    addPuzzle(scene, lane);
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
                constexpr auto StatsKey = KeyCode::I;

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
                if(input.IsKeyPressed(StatsKey))
                    Debug().showStats = !Debug().showStats;
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
                uiRenderer = std::make_unique<UIRenderer>(
                    device,
                    formats.color,
                    formats.depth
                );

                LOG_INFO(
                    "PhysicsPlayground",
                    "R restarts the solutions, C the controls, F frames the "
                    "puzzles, L toggles the labels, I the frame stats"
                );
            }

            // the labels first, so the chip and the stats draw over them
            std::span<const RHITextureBarrier> OnPrepareUI(
                RHICommandList& cmdList
            ) override {
                if(showLabels)
                    drawLabels();
                if(auto* port = Port())
                    drawPortStatusChip(port->Status());
                if(Debug().showStats)
                    statsOverlay.Draw(LastFrameStats());
                uiRenderer->Prepare(cmdList);

                return uiRenderer->TextureAcquires();
            }

            void OnRecordUI(RHICommandList& cmdList) override {
                uiRenderer->Record(cmdList);
            }

        private:
            static Config makeConfig() {
                return Config{
                    .clearColor = SkyRadiance,
                    .drawCapacity = 256,
                    .materialCapacity = 64,
                    .vertexPoolCapacity = 4096,
                    .indexPoolCapacity = 16384
                };
            }

            static FlyCamera::Config makeCamera() {
                return FlyCamera::Config{
                    .position = PuzzleViewPosition,
                    .yaw = 0.0f,
                    .pitch = PuzzleViewPitch,
                    .fovY = std::numbers::pi_v<f32> / 3,
                    .nearZ = 0.05f,
                    .farZ = 100.0f,
                    .moveSpeed = 5.0f
                };
            }

            void frameThePuzzles() {
                auto& camera = static_cast<FlyCamera&>(Camera());
                camera.position = PuzzleViewPosition;
                camera.yaw = 0.0f;
                camera.pitch = PuzzleViewPitch;
                camera.RecomputeView();
            }

            // Godot's editor-preview sun and sky, as Playground lights its map
            void addLight(RenderScene& scene) {
                scene.Lights().Add(
                    LightSnapshot{
                        .castShadow = true,
                        .color = ones(),
                        .intensity = 3.0f,
                        .direction = -normalize(Vec3{0.25f, 0.866f, -0.433f})
                    }
                );
                scene.Environment() = EnvironmentSnapshot{
                    .skyAmbient = {0.123f, 0.174f, 0.262f},
                    .groundAmbient = {0.033f, 0.024f, 0.016f}
                };
            }

            void addMaterials(RenderScene& scene) {
                using enum PieceRole;

                const auto opaque = basePipeline(StandardForward, "fs_opaque");
                auto add = [&](const MaterialData& data) {
                    return scene.Materials().Add(
                        MaterialResource{.data = data, .pipeline = opaque}
                    );
                };
                auto role = [&](PieceRole piece) -> auto& {
                    return materials.roles[static_cast<usize>(piece)];
                };

                materials.floor = scene.Materials().Add(
                    MaterialResource{
                        .data =
                            {.albedo = {0.5f, 0.5f, 0.5f}, .roughness = 0.8f},
                        .pipeline = basePipeline(
                            "Engine/Render/Sample/PhysicsPlayground.slang",
                            "fs_grid"
                        )
                    }
                );
                materials.tile =
                    add({.albedo = {0.16f, 0.18f, 0.22f}, .roughness = 0.9f});
                materials.tether =
                    add({.albedo = {0.20f, 0.20f, 0.22f}, .roughness = 0.6f});
                materials.water = scene.Materials().Add(
                    MaterialResource{
                        .data = {
                            .albedo = {0.25f, 0.50f, 0.85f},
                            .roughness = 0.1f,
                            .opacity = 0.35f
                        },
                        .pipeline = translucentPipeline(StandardForward)
                    }
                );
                role(Ground) =
                    add({.albedo = {0.66f, 0.63f, 0.58f}, .roughness = 0.8f});
                role(Wall) =
                    add({.albedo = {0.40f, 0.45f, 0.52f}, .roughness = 0.7f});
                role(Mover) =
                    add({.albedo = {0.90f, 0.38f, 0.12f}, .roughness = 0.5f});
                role(Lever) =
                    add({.albedo = {0.55f, 0.36f, 0.20f}, .roughness = 0.7f});
                role(Weight) = add(
                    {.albedo = {0.30f, 0.30f, 0.34f},
                     .metallic = 1.0f,
                     .roughness = 0.4f}
                );
                role(Payload) =
                    add({.albedo = {0.95f, 0.95f, 0.92f}, .roughness = 0.3f});
                for(auto& lane: lanes) {
                    lane.goalMaterial = add(
                        {.albedo = {0.15f, 0.55f, 0.22f}, .roughness = 0.5f}
                    );
                }
            }

            // its bounds reach the lanes' ceiling and below the sink, so the
            // shadow fit, the union of every visible bound, holds still
            void addFloor(RenderScene& scene) {
                constexpr Vec3 FloorCenter{0.0f, 0.0f, 3.0f};
                // resting contacts sink up to the 0.02 m slop, landings more
                constexpr f32 Sink = 0.05f;

                const auto mesh = addMeshResource(
                    scene,
                    floorPlane,
                    materials.floor,
                    AABB3D{.halfScale = FloorHalf}
                );
                scene.Primitives().Add(
                    PrimitiveSnapshot{
                        .localToWorld = translateMat(FloorCenter),
                        .worldBounds = AABB3D{
                            .center = FloorCenter +
                                Vec3{0.0f, 0.5f * (PuzzleHeight - Sink), 0.0f},
                            .halfScale = FloorHalf +
                                Vec3{0.0f, 0.5f * (PuzzleHeight + Sink), 0.0f}
                        },
                        .mesh = mesh
                    }
                );
            }

            // a flat mark on a surface, a unit quad stretched in x and z
            void addMark(
                RenderScene& scene,
                MaterialHandle material,
                Vec3 center,
                Vec2 halfExtent
            ) {
                const auto mesh = addMeshResource(
                    scene,
                    quad,
                    material,
                    AABB3D{.halfScale = {0.5f, 0.0f, 0.5f}}
                );
                addStatic(
                    scene,
                    mesh,
                    translateMat(center) * scaleMat(
                        {2.0f * halfExtent.x, 1.0f, 2.0f * halfExtent.y}
                    ),
                    combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
                );
            }

            PrimitiveHandle addMoving(
                RenderScene& scene,
                const GeometryAllocation& geometry,
                MaterialHandle material
            ) {
                const auto mesh =
                    addMeshResource(scene, geometry, material, UnitMeshBounds);

                return scene.Primitives().Add(PrimitiveSnapshot{.mesh = mesh});
            }

            void addPuzzle(RenderScene& scene, PuzzleLane& lane) {
                constexpr Vec3 WaterInset{0.001f, 0.001f, 0.001f};

                const auto& puzzle = lane.session->GetPuzzle();
                addMark(
                    scene,
                    materials.tile,
                    lane.origin + Vec3{0.0f, 0.004f, 0.0f},
                    {PuzzleHalfWidth, PuzzleHalfWidth}
                );
                // translucent and shadowless, a hair inside its tank
                for(const auto& water: puzzle.waters) {
                    addStatic(
                        scene,
                        addMeshResource(
                            scene,
                            box,
                            materials.water,
                            UnitMeshBounds
                        ),
                        translateMat(lane.origin + water.center) *
                            scaleMat(2.0f * (water.halfExtent - WaterInset)),
                        PrimitiveFlags::Visible
                    );
                }

                lane.bodies.clear();
                for(u32 i = 0; i < puzzle.bodies.size(); ++i) {
                    if(isDrawn(puzzle.bodies[i]))
                        addBody(scene, lane, i);
                }

                lane.tethers.clear();
                for(u32 i = 0; i < puzzle.hinges.size(); ++i) {
                    if(!puzzle.hinges[i].tethered)
                        continue;
                    lane.tethers.push_back(
                        LaneTether{
                            .hinge = i,
                            .primitive =
                                addMoving(scene, box, materials.tether),
                        }
                    );
                }
            }

            // a compound draws a box per part
            void addBody(RenderScene& scene, PuzzleLane& lane, u32 index) {
                const auto& body = lane.session->GetPuzzle().bodies[index];
                const auto material = body.role == PieceRole::Goal
                    ? lane.goalMaterial
                    : materials.roles[static_cast<usize>(body.role)];
                const auto& shape = body.desc.shape;
                if(const auto* compound = std::get_if<CompoundShape>(&shape)) {
                    for(const auto& part: compound->parts) {
                        lane.bodies.push_back(
                            LaneBody{
                                .body = index,
                                .primitive = addMoving(scene, box, material),
                                .part = part.pose,
                                .meshScale = 2.0f * part.halfExtent,
                            }
                        );
                    }
                    return;
                }

                const auto isBall = std::holds_alternative<SphereShape>(shape);
                lane.bodies.push_back(
                    LaneBody{
                        .body = index,
                        .primitive =
                            addMoving(scene, isBall ? ball : box, material),
                        .meshScale = unitMeshScaleOf(shape),
                    }
                );
            }

            void bindBodies(PuzzleLane& lane) {
                const auto& session = *lane.session;
                const auto& hinges = session.GetPuzzle().hinges;

                lane.sync = BodyPrimitiveSync(lane.origin);
                for(const auto& body: lane.bodies) {
                    lane.sync.Bind(
                        BodyBinding{
                            .body = session.HandleOf(body.body),
                            .primitive = body.primitive,
                            .part = body.part,
                            .meshScale = body.meshScale,
                        }
                    );
                }
                for(const auto& tether: lane.tethers) {
                    const auto& hinge = hinges[tether.hinge];
                    lane.sync.Bind(
                        TetherBinding{
                            .hinge = session.HingeHandleOf(tether.hinge),
                            .body = session.HandleOf(hinge.body),
                            .primitive = tether.primitive,
                            .anchor = hinge.desc.pivot,
                            .axis = hinge.desc.axis,
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
                const auto viewProj = Camera().ViewProj(Aspect());
                const auto* viewport = ImGui::GetMainViewport();
                auto* drawList = ImGui::GetBackgroundDrawList();

                for(const auto& lane: lanes) {
                    const auto anchor = lane.origin +
                        Vec3{0.0f, 0.0f, -PuzzleHalfWidth - 0.15f};
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
                    const auto task = Str(lane.session->GetPuzzle().task);
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
                         ImGui::CalcTextSize(task.c_str()).x,
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
                        task.c_str()
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
        .SetProperty("eventsApplied", &PuzzleStatusMirror::eventsApplied)
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
