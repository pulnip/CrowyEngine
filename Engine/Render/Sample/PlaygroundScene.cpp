#include "PlaygroundScene.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <utility>

#include <imgui.h>

#include "Assert.hpp"
#include "CameraComponent.hpp"
#include "ClassRegistry.hpp"
#include "Expected.hpp"
#include "FlyControl.hpp"
#include "LightComponent.hpp"
#include "Log.hpp"
#include "PortStatusChip.hpp"
#include "ProjectRoot.hpp"
#include "PropertyWalker.hpp"
#include "Scene.hpp"
#include "ShadingModel.hpp"
#include "SnapshotCamera.hpp"

namespace Crowy
{
    namespace
    {
        constexpr StrView MaterialFolder = "engine:Render/Sample/Playground/";

        MaterialAssetHandle loadMaterial(MaterialStore& store, StrView name) {
            const auto ref =
                AssetRef::Parse(std::format("{}{}.json", MaterialFolder, name));

            return valueOrThrow(store.Load(ref.value()));
        }

        PlaygroundMaterials loadMaterials(MaterialStore& store) {
            const auto load = [&](StrView name) {
                return loadMaterial(store, name);
            };
            const auto chart = [&] {
                constexpr auto Columns = PlaygroundChartColumns;

                decltype(PlaygroundMaterials::chart) handles;
                for(u32 i = 0; i < handles.size(); ++i) {
                    handles[i] = load(
                        std::format(
                            "{}{}",
                            i < Columns ? "Dielectric" : "Metal",
                            i % Columns
                        )
                    );
                }
                return handles;
            };

            return PlaygroundMaterials{
                .floor = load("Floor"),
                .wall = load("Wall"),
                .block = load("Block"),
                .emissive =
                    {load("Emissive1"), load("Emissive4"), load("Emissive16")},
                .chart = chart(),
                .bars = {load("BarRed"), load("BarGreen"), load("BarBlue")},
                .doubleSided = load("DoubleSided"),
                .translucent = load("Glass")
            };
        }

        // the first actor of that name; the file names each one the code
        // reaches
        ActorId findActor(const World& world, StrView name) {
            auto found = ActorId::InvalidHandle();
            world.EachActor([&](ActorId actor, ActorId) {
                if(!found.IsValid() && world.Name(actor) == name)
                    found = actor;
            });
            if(!found.IsValid()) {
                throw std::runtime_error(
                    std::format("the Playground scene has no actor '{}'", name)
                );
            }

            return found;
        }
    }

    PlaygroundScene::PlaygroundScene(const Config& config)
        : RenderApp(config, std::make_unique<ViewCamera>()) {}

    RenderApp::Config PlaygroundScene::MakeConfig() {
        return Config{
            .clearColor = SkyRadiance,
            .drawCapacity = 64,
            .materialCapacity = 64,
            .vertexPoolCapacity = 4096,
            .indexPoolCapacity = 16384,
            .showsSceneView = true
        };
    }

    FlyCamera::Config PlaygroundScene::MakeCamera() {
        const auto overview = *poseOf(CameraBookmark::Overview);

        return FlyCamera::Config{
            .position = overview.position,
            .yaw = overview.yaw,
            .pitch = overview.pitch,
            .fovY = std::numbers::pi_v<f32> / 3,
            .nearZ = 0.05f,
            .farZ = 100.0f,
            .moveSpeed = 5.0f
        };
    }

    EditorCut PlaygroundScene::MakeSceneCut() {
        const auto start = MakeCamera();

        return EditorCut{
            .name = "scene",
            .position = start.position,
            .yaw = start.yaw,
            .pitch = start.pitch,
            .lens =
                {.fovY = start.fovY, .nearZ = start.nearZ, .farZ = start.farZ}
        };
    }

    void PlaygroundScene::extractScene(RenderScene& scene) {
        world.Provide(scene);
        world.Provide(Meshes());
        world.Provide(Materials());
        world.Provide(primitiveActors);
        world.Provide(static_cast<SnapshotCamera&>(Camera()).Snapshot());
        // Godot's editor-preview sky and ground colors, decoded to linear
        scene.Environment() = EnvironmentSnapshot{
            .skyAmbient = {0.123f, 0.174f, 0.262f},
            .groundAmbient = {0.033f, 0.024f, 0.016f}
        };

        loadSceneFile(
            world,
            resolveProjectPath(
                "Engine/Render/Sample/Playground/Playground.json"
            )
        );
        materials = loadMaterials(Materials());
        restyleMaterials(scene, materials, shading);
        // the actors' rows land, so the panel and the port find them
        world.Frame(0.0);

        const auto start = MakeCamera();
        fly = FlyState(CameraLook{.yaw = start.yaw, .pitch = start.pitch});
        cameraActor = findActor(world, "camera");
        [[maybe_unused]] const auto& lens =
            *world.Get<CameraComponent>(cameraActor);
        CROWY_ASSERT(
            lens.nearZ == start.nearZ && lens.farZ == start.farZ,
            "the file's depth range is the one describePipeline read"
        );
        CROWY_ASSERT(
            world.LocalTransform(cameraActor).rotation ==
                flyRotation(fly.look.yaw, fly.look.pitch),
            "the file's camera looks where the fly look starts"
        );

        sceneView.emplace(world, scene, MakeSceneCut(), Port());
        sceneView->Camera().lookSensitivity = start.lookSensitivity;
        sceneView->Camera().moveSpeed = start.moveSpeed;

        // the lamp is off in the file, so the baseline never sees it
        inspect(scene, findActor(world, "sun"), findActor(world, "lamp"));
        shownDebug = Debug();
        panel = buildPanel();
    }

    void PlaygroundScene::pollInput(const InputProvider& input) {
        auto& debug = Debug();
        if(input.IsKeyPressed(PanelToggleKey)) {
            debug.showPanel = !debug.showPanel;
        }
        if(input.IsKeyPressed(StatsToggleKey)) {
            debug.showStats = !debug.showStats;
        }

        const auto viewRuns = SceneViewID() != 0;
        // the cursor hides while it turns, so a look never meets a screen edge
        const auto looking = sceneView->PollInput(input, viewRuns);
        if(viewRuns) {
            if(looking)
                RequestRelativeMouse();
        }
        else if(fly.PollInput(input, MakeCamera().lookSensitivity)) {
            RequestRelativeMouse();
        }
    }

    void PlaygroundScene::updateScene(f64 seconds) {
        const auto sceneViewRuns = SceneViewID() != 0;
        sceneView->Update(
            seconds,
            static_cast<ViewCamera&>(Camera()),
            sceneViewRuns
        );
        // a still camera writes nothing, so nothing is extracted for it
        if(!sceneViewRuns && fly.Moving()) {
            world.SetLocalTransform(
                cameraActor,
                fly.Step(
                    world.LocalTransform(cameraActor),
                    MakeCamera().moveSpeed,
                    seconds
                )
            );
        }
        world.Frame(seconds);
    }

    void PlaygroundScene::initUI(
        RHIDevice& device,
        const OverlayFormats& formats
    ) {
        uiRenderer =
            std::make_unique<UIRenderer>(device, formats.color, formats.depth);

        LOG_INFO(
            "Playground",
            "P toggles the inspector panel, I the frame stats"
        );
    }

    std::span<const RHITextureBarrier> PlaygroundScene::prepareUI(
        RHICommandList& cmdList
    ) {
        // their own windows, closed before the SetNextWindow* below land
        if(auto* port = Port()) {
            drawPortStatusChip(port->Status());
        }
        const auto& debug = Debug();
        if(debug.showStats) {
            statsOverlay.Draw(LastFrameStats());
        }

        // the port, the keys or the panel itself may have moved it
        if(debug != shownDebug) {
            shownDebug = debug;
            uiContext.panelDirty = true;
        }
        // here, before Prepare: a callback runs inside the tree's submit
        // and may only raise the flag
        if(std::exchange(uiContext.panelDirty, false))
            panel = buildPanel();

        // the running list's, not debug's, which the panel may flip mid-frame
        if(const auto size = sceneView->DrawWindow(SceneViewID())) {
            SetSceneViewSize(size->x, size->y);
        }
        if(sceneView->UpdateOverlay())
            uiContext.panelDirty = true;

        if(debug.showPanel) {
            // Prepare opens the shared "Crowy" window, whose saved rect
            // another sample may have left collapsed or off-screen
            ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(
                ImVec2(360.0f, 640.0f),
                ImGuiCond_Appearing
            );
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);

            uiRenderer->Prepare(cmdList, panel, uiContext);
        }
        else {
            uiRenderer->Prepare(cmdList);
        }

        return uiRenderer->TextureAcquires();
    }

    void PlaygroundScene::recordUI(RHICommandList& cmdList) {
        uiRenderer->Record(cmdList);
    }

    PropertyTarget PlaygroundScene::materialTarget(MaterialHandle handle) {
        return PropertyTarget(
            [this, handle]() -> const void* {
                const auto& materials = Scene().Materials();
                return materials.IsValid(handle)
                           ? &materials.GetRef(handle).data
                           : nullptr;
            },
            [this, handle]() -> void* {
                auto& materials = Scene().Materials();
                return materials.IsValid(handle)
                           ? &materials.GetRef(handle).data
                           : nullptr;
            },
            *getDesc<MaterialData>()
        );
    }

    CStr PlaygroundScene::moduleOf(ChartShading chart) {
        using enum ChartShading;

        switch(chart) {
        case PBR:
            return PBRShadingModule;
        case Toon:
            return ToonShadingModule;
        case Unlit:
            return UnlitShadingModule;
        }

        return PBRShadingModule;
    }

    std::optional<PlaygroundScene::CameraPose> PlaygroundScene::poseOf(
        CameraBookmark bookmark
    ) {
        using enum CameraBookmark;

        switch(bookmark) {
        case Free:
            return std::nullopt;
        case Overview:
            return CameraPose{{0.0f, 1.7f, -5.0f}, 0.0f, 0.1f};
        case Scale:
            return CameraPose{{-7.125f, 1.0f, 2.0f}, 0.0f, 0.2f};
        case Emissive:
            return CameraPose{{-4.25f, 1.25f, 0.75f}, 0.0f, 0.05f};
        case Materials:
            return CameraPose{{0.0f, 1.5f, 0.25f}, 0.0f, 0.1f};
        case Glass:
            return CameraPose{{4.25f, 1.25f, 0.75f}, 0.0f, 0.05f};
        case DoubleSided:
            return CameraPose{{7.125f, 1.0f, 1.25f}, 0.0f, 0.05f};
        }

        return std::nullopt;
    }

    Widget PlaygroundScene::buildPanel() {
        // one row can show in two sections, whose fields each write it whole
        const auto refreshing = [this](const DirtyCallback& apply) {
            return DirtyCallback([this, apply] {
                if(apply)
                    apply();
                uiContext.panelDirty = true;
            });
        };

        const auto overrides = sceneView->Overrides();

        std::vector<Widget> sections{debugSection()};
        for(const auto& target: inspected) {
            sections.push_back(buildPropertyTree(
                target.label.c_str(),
                target.target,
                refreshing(target.onChange),
                overrides
            ));
        }
        if(auto selection = sceneView->SelectionSection(refreshing({})))
            sections.push_back(std::move(*selection));

        return Column(std::move(sections));
    }

    Widget PlaygroundScene::debugSection() {
        // Record reads it every frame; no dirty consumer
        return buildPropertyTree(
            "debug",
            &Debug(),
            *getDesc<RenderDebug>(),
            [] {}
        );
    }

    void PlaygroundScene::inspect(
        RenderScene& scene,
        ActorId sun,
        ActorId lamp
    ) {
        constexpr auto Columns = PlaygroundChartColumns;
        constexpr auto Last = Columns - 1;
        // the World extracts what changed, and the draw lists and BeginFrame
        // re-read the rest every frame
        const DirtyCallback none = [] {
        };

        inspected = {
            InspectedTarget{
                "shading",
                "shading",
                PropertyTarget::Fixed(&shading, *getDesc<PlaygroundShading>()),
                [this] { linkChartShading(); }
            },
            InspectedTarget{
                "bookmarks",
                "bookmarks",
                PropertyTarget::Fixed(&bookmarks, *getDesc<CameraBookmarks>()),
                [this] { snapCamera(); }
            },
            InspectedTarget{
                "camera",
                "camera",
                world.Target<CameraComponent>(cameraActor),
                none
            },
            // a rotation written here lasts until the next mouse look, which
            // turns from `camera.look`
            InspectedTarget{
                "camera.transform",
                "camera transform",
                world.TransformTarget(cameraActor),
                none
            },
            InspectedTarget{
                "camera.look",
                "camera look",
                PropertyTarget::Fixed(&fly.look, *getDesc<CameraLook>()),
                [this] { applyLook(); }
            },
        };
        for(const auto [name, actor]:
            {std::pair{"sun", sun}, std::pair{"lamp", lamp}}) {
            inspected.push_back(
                InspectedTarget{
                    std::format("light.{}", name),
                    name,
                    world.Target<LightComponent>(actor),
                    none
                }
            );
            inspected.push_back(
                InspectedTarget{
                    std::format("light.{}.transform", name),
                    std::format("{} transform", name),
                    world.TransformTarget(actor),
                    none
                }
            );
        }
        inspected.push_back(
            InspectedTarget{
                "environment",
                "environment",
                PropertyTarget::Fixed(
                    &scene.Environment(),
                    *getDesc<EnvironmentSnapshot>()
                ),
                none
            }
        );
        for(const auto [name, handle]:
            {std::pair{"floor", materials.floor->row},
             std::pair{"smooth-dielectric", materials.chart[0]->row},
             std::pair{"rough-dielectric", materials.chart[Last]->row},
             std::pair{"smooth-metal", materials.chart[Columns]->row},
             std::pair{"rough-metal", materials.chart[Columns + Last]->row},
             std::pair{"double-sided", materials.doubleSided->row},
             std::pair{"translucent", materials.translucent->row},
             // the middle panel, at 4
             std::pair{"emissive", materials.emissive[1]->row}}) {
            inspected.push_back(
                InspectedTarget{
                    std::format("material.{}", name),
                    name,
                    materialTarget(handle),
                    none
                }
            );
        }

        exposures = PortExposures(Port());
        for(const auto& target: inspected) {
            exposures.Expose(
                target.portName,
                target.target,
                [this, onChange = target.onChange] {
                    onChange();
                    uiContext.panelDirty = true;
                }
            );
        }
    }

    void PlaygroundScene::linkChartShading() {
        for(const auto& handle: materials.chart) {
            Scene().Materials().GetRef(handle->row).pipeline.shadingModule =
                moduleOf(shading.chart);
        }
    }

    void PlaygroundScene::snapCamera() {
        const auto pose = poseOf(bookmarks.current);
        if(!pose)
            return;

        if(SceneViewID() != 0) {
            auto& camera = sceneView->Camera();
            camera.position = pose->position;
            camera.yaw = pose->yaw;
            camera.pitch = pose->pitch;
            camera.RecomputeView();
            return;
        }

        fly.look = CameraLook{.yaw = pose->yaw, .pitch = pose->pitch};
        world.SetLocalTransform(
            cameraActor,
            Transform{
                .position = pose->position,
                .rotation = flyRotation(fly.look.yaw, fly.look.pitch)
            }
        );
        uiContext.panelDirty = true;
    }

    void PlaygroundScene::applyLook() {
        auto local = world.LocalTransform(cameraActor);
        local.rotation = flyRotation(fly.look.yaw, fly.look.pitch);
        world.SetLocalTransform(cameraActor, local);
    }

    // clang-format off
    CROWY_STRUCT(CameraBookmarks)
        .AddProperty("current", &CameraBookmarks::current)
    CROWY_STRUCT_END(CameraBookmarks)

    CROWY_STRUCT(PlaygroundShading)
        .AddProperty("chart", &PlaygroundShading::chart)
    CROWY_STRUCT_END(PlaygroundShading)

    CROWY_STRUCT(CameraLook)
        .AddProperty("yaw", &CameraLook::yaw)
        .AddProperty("pitch", &CameraLook::pitch)
        .SetUIRange(-1.55f, 1.55f)
    CROWY_STRUCT_END(CameraLook)
    // clang-format on
}
