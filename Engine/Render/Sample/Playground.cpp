#include <array>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <imgui.h>

#include "EnumUtil.hpp"
#include "FlyCamera.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Log.hpp"
#include "MeshGenerator.hpp"
#include "Object.hpp"
#include "PortStatusChip.hpp"
#include "PropertyWalker.hpp"
#include "RenderApp.hpp"
#include "StatsOverlay.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    struct UIContext {
        // a value changed behind the panel's widgets, which seed only when
        // they are built
        bool panelDirty = false;
    };

    // a named camera pose per station; Free leaves the camera where it is
    enum class CameraBookmark : u32 {
        Free,
        Overview,
        Scale,
        Emissive,
        Materials,
        Glass,
        DoubleSided,
    };

    CROWY_ENUM_BEGIN(CameraBookmark)
        CROWY_ENUM_VALUE(Free)
        CROWY_ENUM_VALUE(Overview)
        CROWY_ENUM_VALUE(Scale)
        CROWY_ENUM_VALUE(Emissive)
        CROWY_ENUM_VALUE(Materials)
        CROWY_ENUM_VALUE(Glass)
        CROWY_ENUM_VALUE(DoubleSided)
    CROWY_ENUM_END()

    // exposed as `bookmarks`; writing `current` snaps the camera, even to
    // the bookmark it already names
    struct CameraBookmarks {
        CameraBookmark current = CameraBookmark::Overview;
    };

    CROWY_STRUCT(CameraBookmarks)
        .SetProperty("current", &CameraBookmarks::current)
    CROWY_STRUCT_END(CameraBookmarks)

    CROWY_STRUCT(LightSnapshot)
        .SetProperty("kind", &LightSnapshot::kind)
        .SetProperty("enabled", &LightSnapshot::enabled)
        .SetProperty("castShadow", &LightSnapshot::castShadow)
        .SetProperty("color", &LightSnapshot::color)
        .SetProperty("intensity", &LightSnapshot::intensity)
        .SetUIRange(0.0f, 16.0f)
        .SetProperty("position", &LightSnapshot::position)
        .SetProperty("direction", &LightSnapshot::direction)
        .SetProperty("range", &LightSnapshot::range)
        .SetUIRange(0.0f, 20.0f)
        .SetProperty("innerConeAngle", &LightSnapshot::innerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .SetProperty("outerConeAngle", &LightSnapshot::outerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .SetProperty("shadowBias", &LightSnapshot::shadowBias)
        .SetUIRange(0.0f, 0.1f)
        .SetProperty("shadowNormalBias", &LightSnapshot::shadowNormalBias)
        .SetUIRange(0.0f, 4.0f)
    CROWY_STRUCT_END(LightSnapshot)

    CROWY_STRUCT(EnvironmentSnapshot)
        .SetProperty("skyAmbient", &EnvironmentSnapshot::skyAmbient)
        .SetProperty("groundAmbient", &EnvironmentSnapshot::groundAmbient)
    CROWY_STRUCT_END(EnvironmentSnapshot)

    CROWY_STRUCT(MaterialData)
        .SetProperty("albedo", &MaterialData::albedo)
        .SetProperty("metallic", &MaterialData::metallic)
        .SetUIRange(0.0f, 1.0f)
        .SetProperty("emissive", &MaterialData::emissive)
        .SetProperty("roughness", &MaterialData::roughness)
        .SetUIRange(0.0f, 1.0f) CROWY_STRUCT_END(MaterialData)

            CROWY_STRUCT(FlyCamera)
        .SetProperty("position", &FlyCamera::position)
        .SetProperty("yaw", &FlyCamera::yaw)
        .SetProperty("pitch", &FlyCamera::pitch)
        .SetUIRange(-1.55f, 1.55f)
        .SetProperty("fovY", &FlyCamera::config, &FlyCamera::Config::fovY)
        .SetUIRange(0.35f, 2.4f) CROWY_STRUCT_END(FlyCamera)

        // The prototyping map, in metres: a row of test stations across the
        // back of a walled floor, each testing one thing, fronts on one line,
        // every box face on the 0.25 m grid the floor draws. Nothing floats.
        // Each pass's draw list orders its own draws: opaque ones by pipeline,
        // then near first, the glass far first.
        class Playground: public RenderApp {
        // the line every station's front stands on
        static constexpr f32 FrontZ = 3.5f;

        // dielectric in front, metal behind: a real surface is one or the
        // other, so metallic takes no values between
        static constexpr u32 ChartRows = 2;
        // roughness 0, 0.25, 0.5, 0.75, 1 from left to right
        static constexpr u32 ChartColumns = 5;
        static constexpr f32 SphereRadius = 0.25f;
        // one colour for both rows, so metallic is all that differs; an
        // orange, which a metal's highlight takes
        static constexpr Vec3 ChartAlbedo{0.85f, 0.55f, 0.30f};

        // Godot's editor-preview sky as radiance: the tone map takes it back
        // to the bytes (98, 116, 140) it was written as, 0.4 of a step clear
        // of every rounding edge
        static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};

        // the engine's own colour passes, by path
        static constexpr CStr StandardForward =
            "Engine/Render/Shader/StandardForward.slang";

        static constexpr auto PanelToggleKey = KeyCode::P;
        static constexpr auto StatsToggleKey = KeyCode::I;

        struct NamedMaterial {
            CStr name;
            MaterialHandle handle;
        };

        struct NamedLight {
            CStr name;
            LightHandle handle;
        };

        struct CameraPose {
            Vec3 position;
            f32 yaw = 0.0f;
            f32 pitch = 0.0f;
        };

        GeometryAllocation sphere{};
        // stretched into walls, boards and bars, scaled into blocks
        GeometryAllocation unitBox{};
        GeometryAllocation floorPlane{};
        // unit quads; the one facing away only shows where nothing is culled
        GeometryAllocation quadFacingCamera{};
        GeometryAllocation quadFacingAway{};

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        // the panel's light and material sections, and the port's targets
        // besides `camera` and `environment`
        std::vector<NamedLight> exposedLights;
        std::vector<NamedMaterial> exposedMaterials;
        // what the panel's debug section was built from
        RenderDebug shownDebug;
        CameraBookmarks bookmarks;

        // shown by debug.showStats, hidden by default like the panel so the
        // smoke capture matches the panel-less one
        StatsOverlay statsOverlay;

    public:
        // the port outlives these members, and its callbacks point at them
        ~Playground() override {
            if(auto* port = Port()) {
                port->Unexpose("bookmarks");
                port->Unexpose("camera");
                port->Unexpose("environment");
                for(const auto& light: exposedLights)
                    port->Unexpose(std::format("light.{}", light.name));
                for(const auto& material: exposedMaterials)
                    port->Unexpose(std::format("material.{}", material.name));
            }
        }
        CROWY_DECLARE_PINNED(Playground)

        Playground()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            const auto sphereMesh = MakeSphere(SphereRadius, 48, 24);
            const auto boxMesh = MakeBox(0.5f);
            const auto floorMesh = MakePlane(Vec2{10.0f, 10.0f});
            const auto towardMesh = MakePlane(-unitZ(), unitX(), 0.5f);
            const auto awayMesh = MakePlane(unitZ(), unitX(), 0.5f);

            sphere = pool.Add(sphereMesh.vertices, sphereMesh.indices);
            unitBox = pool.Add(boxMesh.vertices, boxMesh.indices);
            floorPlane = pool.Add(floorMesh.vertices, floorMesh.indices);
            quadFacingCamera =
                pool.Add(towardMesh.vertices, towardMesh.indices);
            quadFacingAway = pool.Add(awayMesh.vertices, awayMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            const auto boxHalf = 0.5f * ones();
            constexpr Vec3 QuadHalf{0.5f, 0.5f, 0.0f};

            // every row before any address is taken: the table is a vector,
            // so a later Add could move the rows the panel and port point at

            // Godot's editor-preview sun: 60 degrees up, 30 degrees east of
            // north, with north behind the start camera, so it lights the
            // map from the front
            const auto sun = scene.Lights().Add(
                LightSnapshot{
                    .castShadow = true,
                    .color = ones(),
                    .intensity = 3.0f,
                    .direction = -normalize(Vec3{0.25f, 0.866f, -0.433f})
                }
            );
            // off until the port or the panel turns it on, so the golden
            // never sees it; aimed down at the chart for when it is a spot
            const auto lamp = scene.Lights().Add(
                LightSnapshot{
                    .kind = LightKind::Point,
                    .enabled = false,
                    .color = {1.0f, 0.8f, 0.6f},
                    .intensity = 4.0f,
                    .position = {0.0f, 1.5f, 2.5f},
                    .direction = {0.0f, -0.45f, 0.9f},
                    .range = 4.0f
                }
            );
            // the same preview's sky and ground colours, decoded to linear
            scene.Environment() = EnvironmentSnapshot{
                .skyAmbient = {0.123f, 0.174f, 0.262f},
                .groundAmbient = {0.033f, 0.024f, 0.016f}
            };

            const auto floorMaterial = addMaterial(
                scene,
                MaterialData{.albedo = {0.5f, 0.5f, 0.5f}, .roughness = 0.8f},
                gridPipeline()
            );
            const auto wallMaterial = addMaterial(
                scene,
                MaterialData{.albedo = {0.62f, 0.62f, 0.6f}, .roughness = 0.8f},
                gridPipeline()
            );
            // the scale blocks and the chart's step
            const auto blockMaterial = addMaterial(
                scene,
                MaterialData{
                    .albedo = {0.55f, 0.62f, 0.72f},
                    .roughness = 0.6f
                },
                gridPipeline()
            );
            // 1, 4 and 16, so the tone mapper's compression shows
            std::array<MaterialHandle, 3> emissiveMaterials;
            for(u32 i = 0; i < emissiveMaterials.size(); ++i) {
                const auto radiance = static_cast<f32>(1u << (2 * i));

                emissiveMaterials[i] = addMaterial(
                    scene,
                    MaterialData{
                        .albedo = zeros(),
                        .emissive = radiance * ones()
                    },
                    opaquePipeline()
                );
            }

            std::array<MaterialHandle, ChartRows * ChartColumns> chartMaterials;
            for(u32 row = 0; row < ChartRows; ++row) {
                for(u32 column = 0; column < ChartColumns; ++column) {
                    constexpr auto Step = 1.0f / (ChartColumns - 1);

                    chartMaterials[row * ChartColumns + column] = addMaterial(
                        scene,
                        MaterialData{
                            .albedo = ChartAlbedo,
                            .metallic = static_cast<f32>(row),
                            .roughness = static_cast<f32>(column) * Step
                        },
                        opaquePipeline()
                    );
                }
            }

            // red, green and blue behind the glass
            const std::array barColors{
                Vec3{0.8f, 0.1f, 0.1f},
                Vec3{0.1f, 0.6f, 0.15f},
                Vec3{0.1f, 0.2f, 0.8f}
            };
            std::array<MaterialHandle, 3> barMaterials;
            for(u32 i = 0; i < barMaterials.size(); ++i) {
                barMaterials[i] = addMaterial(
                    scene,
                    MaterialData{.albedo = barColors[i]},
                    opaquePipeline()
                );
            }

            const auto doubleSided = addMaterial(
                scene,
                MaterialData{.albedo = {0.35f, 0.70f, 0.45f}},
                doubleSidedPipeline()
            );
            const auto translucent = addMaterial(
                scene,
                MaterialData{.albedo = {0.35f, 0.55f, 0.95f}, .opacity = 0.45f},
                translucentPipeline()
            );

            // grid: the floor, three walls leaving the camera side open, the
            // scale blocks and the chart's step
            const auto blockMesh =
                addMesh(scene, unitBox, blockMaterial, boxHalf);
            addPrimitive(
                scene,
                addMesh(scene, floorPlane, floorMaterial, {10.0f, 0.0f, 10.0f}),
                zeros(),
                ones()
            );
            const auto wallMesh =
                addMesh(scene, unitBox, wallMaterial, boxHalf);
            addPrimitive(
                scene,
                wallMesh,
                {-10.1f, 2.0f, 0.0f},
                {0.2f, 4.0f, 20.4f}
            );
            addPrimitive(
                scene,
                wallMesh,
                {10.1f, 2.0f, 0.0f},
                {0.2f, 4.0f, 20.4f}
            );
            // nearer centres draw first, so the back wall draws after the side
            // walls, and Equal behind the prepass keeps it on the corner seams
            addPrimitive(
                scene,
                wallMesh,
                {0.0f, 2.0f, 10.1f},
                {20.4f, 4.0f, 0.2f}
            );

            // scale: blocks of 1, 0.5 and 0.25 m, left faces at these x
            for(const auto [left, edge]:
                {std::pair{-8.25f, 1.0f},
                 std::pair{-7.0f, 0.5f},
                 std::pair{-6.25f, 0.25f}}) {
                const auto half = 0.5f * edge;

                addPrimitive(
                    scene,
                    blockMesh,
                    {left + half, half, FrontZ + half},
                    edge * ones()
                );
            }
            // the chart's back row stands a metre up, so it clears the
            // front row from eye height
            addPrimitive(
                scene,
                blockMesh,
                {0.0f, 0.5f, 4.25f},
                {5.0f, 1.0f, 0.5f}
            );

            // opaque. emissive: three 0.5 m panels at the back of a niche
            // 2.5 m wide and 0.5 m deep, whose roof shades the whole band
            // they hang in, so they read against a shadow rather than dark
            // paint; wide enough that the start camera sees every panel whole
            constexpr f32 NicheDepth = 0.5f;
            constexpr f32 NicheZ = FrontZ + 0.5f * NicheDepth;
            // the back, then the two sides, the roof and the sill
            addPrimitive(
                scene,
                blockMesh,
                {-4.25f, 1.0f, FrontZ + NicheDepth + 0.125f},
                {3.0f, 2.0f, 0.25f}
            );
            for(const auto x: {-5.625f, -2.875f}) {
                addPrimitive(
                    scene,
                    blockMesh,
                    {x, 1.0f, NicheZ},
                    {0.25f, 2.0f, NicheDepth}
                );
            }
            addPrimitive(
                scene,
                blockMesh,
                {-4.25f, 1.875f, NicheZ},
                {2.5f, 0.25f, NicheDepth}
            );
            addPrimitive(
                scene,
                blockMesh,
                {-4.25f, 0.375f, NicheZ},
                {2.5f, 0.75f, NicheDepth}
            );
            for(u32 i = 0; i < emissiveMaterials.size(); ++i) {
                const auto x = -5.0f + 0.75f * static_cast<f32>(i);

                addPrimitive(
                    scene,
                    addMesh(
                        scene,
                        quadFacingCamera,
                        emissiveMaterials[i],
                        QuadHalf
                    ),
                    {x, 1.25f, FrontZ + NicheDepth - 0.01f},
                    {0.5f, 0.5f, 1.0f}
                );
            }

            // the chart: in each cell a 0.5 m cube carrying a sphere of the
            // same material, so a flat and a curved face show one material
            for(u32 row = 0; row < ChartRows; ++row) {
                const auto base = static_cast<f32>(row);
                const auto z = FrontZ + 0.25f + 0.5f * static_cast<f32>(row);

                for(u32 column = 0; column < ChartColumns; ++column) {
                    const auto material =
                        chartMaterials[row * ChartColumns + column];
                    const auto x = static_cast<f32>(column) - 2.0f;

                    addPrimitive(
                        scene,
                        addMesh(scene, unitBox, material, boxHalf),
                        {x, base + 0.25f, z},
                        0.5f * ones()
                    );
                    addPrimitive(
                        scene,
                        addMesh(scene, sphere, material, SphereRadius * ones()),
                        {x, base + 0.5f + SphereRadius, z},
                        ones()
                    );
                }
            }

            // glass: bars 1 m behind the pane, their tops above it, so each
            // colour shows both through the glass and bare
            for(u32 i = 0; i < barMaterials.size(); ++i) {
                addPrimitive(
                    scene,
                    addMesh(scene, unitBox, barMaterials[i], boxHalf),
                    {3.5f + 0.75f * static_cast<f32>(i), 1.0f, FrontZ + 1.125f},
                    {0.5f, 2.0f, 0.25f}
                );
            }

            // double-sided: a pane facing the camera beside one showing its
            // back, which shows only because nothing is culled; the two
            // should look alike
            addPrimitive(
                scene,
                addMesh(scene, quadFacingCamera, doubleSided, QuadHalf),
                {6.5f, 0.75f, FrontZ},
                {1.0f, 1.5f, 1.0f}
            );
            addPrimitive(
                scene,
                addMesh(scene, quadFacingAway, doubleSided, QuadHalf),
                {7.75f, 0.75f, FrontZ},
                {1.0f, 1.5f, 1.0f}
            );

            // translucent, last: the pane in front of the bars
            addPrimitive(
                scene,
                addMesh(scene, quadFacingCamera, translucent, QuadHalf),
                {4.25f, 0.75f, FrontZ},
                {2.5f, 1.5f, 1.0f}
            );

            exposedLights = {NamedLight{"sun", sun}, NamedLight{"lamp", lamp}};

            constexpr auto Last = ChartColumns - 1;
            exposedMaterials = {
                NamedMaterial{"floor", floorMaterial},
                NamedMaterial{"smooth-dielectric", chartMaterials[0]},
                NamedMaterial{"rough-dielectric", chartMaterials[Last]},
                NamedMaterial{"smooth-metal", chartMaterials[ChartColumns]},
                NamedMaterial{
                    "rough-metal",
                    chartMaterials[ChartColumns + Last]
                },
                NamedMaterial{"double-sided", doubleSided},
                NamedMaterial{"translucent", translucent},
                // the middle panel, at 4
                NamedMaterial{"emissive", emissiveMaterials[1]}
            };

            shownDebug = Debug();
            panel = buildPanel();

            // the port's targets are the panel's, under the same rule; a
            // port write lands behind the panel's widgets, so it rebuilds them
            if(auto* port = Port()) {
                auto& camera = static_cast<FlyCamera&>(Camera());

                port->Expose(
                    "bookmarks",
                    &bookmarks,
                    *GetDesc<CameraBookmarks>(),
                    [this] { snapCamera(); }
                );
                port->Expose(
                    "camera",
                    &camera,
                    *GetDesc<FlyCamera>(),
                    [this, &camera] {
                        camera.RecomputeView();
                        uiContext.panelDirty = true;
                    }
                );
                for(const auto& light: exposedLights) {
                    port->Expose(
                        std::format("light.{}", light.name),
                        &scene.Lights().GetRef(light.handle),
                        *GetDesc<LightSnapshot>(),
                        [this] { uiContext.panelDirty = true; }
                    );
                }
                port->Expose(
                    "environment",
                    &scene.Environment(),
                    *GetDesc<EnvironmentSnapshot>(),
                    [this] { uiContext.panelDirty = true; }
                );
                for(const auto& material: exposedMaterials) {
                    port->Expose(
                        std::format("material.{}", material.name),
                        &materialData(scene, material.handle),
                        *GetDesc<MaterialData>(),
                        [this] { uiContext.panelDirty = true; }
                    );
                }
            }
        }

        void OnProcessInput(const InputProvider& input) override {
            auto& debug = Debug();
            if(input.IsKeyPressed(PanelToggleKey)) {
                debug.showPanel = !debug.showPanel;
            }
            if(input.IsKeyPressed(StatsToggleKey)) {
                debug.showStats = !debug.showStats;
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
                "Playground",
                "P toggles the inspector panel, I the frame stats"
            );
        }

        std::span<const RHITextureBarrier> OnPrepareUI(
            RHICommandList& cmdList
        ) override {
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

            if(debug.showPanel) {
                // Prepare opens the shared "Crowy" window, whose saved rect
                // another sample may have left collapsed or off-screen
                ImGui::SetNextWindowPos(
                    ImVec2(8.0f, 8.0f),
                    ImGuiCond_Appearing
                );
                ImGui::SetNextWindowSize(
                    ImVec2(360.0f, 640.0f),
                    ImGuiCond_Appearing
                );
                ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);

                uiRenderer->Prepare(cmdList, panel, uiContext);
            } else {
                uiRenderer->Prepare(cmdList);
            }

            return uiRenderer->TextureAcquires();
        }

        void OnRecordUI(RHICommandList& cmdList) override {
            uiRenderer->Record(cmdList);
        }

    private:
        // rows do not move after extraction, so the row's address holds as
        // a write-back target for the panel and the port alike
        static MaterialData& materialData(
            RenderScene& scene,
            MaterialHandle handle
        ) {
            return scene.Materials().GetRef(handle).data;
        }

        Widget buildPanel() {
            std::vector<Widget> sections{
                debugSection(),
                bookmarksSection(),
                cameraSection()
            };
            for(const auto& light: exposedLights) {
                sections.push_back(
                    lightSection(Scene(), light.name, light.handle)
                );
            }
            sections.push_back(environmentSection(Scene()));
            for(const auto& material: exposedMaterials) {
                sections.push_back(
                    materialSection(Scene(), material.name, material.handle)
                );
            }

            return Column(std::move(sections));
        }

        Widget materialSection(
            RenderScene& scene,
            CStr label,
            MaterialHandle handle
        ) {
            // the draw lists re-read the table every frame; no dirty consumer
            return buildPropertyTree(
                label,
                &materialData(scene, handle),
                *GetDesc<MaterialData>(),
                [] {}
            );
        }

        Widget lightSection(
            RenderScene& scene,
            CStr label,
            LightHandle handle
        ) {
            // BeginFrame re-reads the table every frame; no dirty consumer
            return buildPropertyTree(
                label,
                &scene.Lights().GetRef(handle),
                *GetDesc<LightSnapshot>(),
                [] {}
            );
        }

        Widget environmentSection(RenderScene& scene) {
            // BeginFrame copies it into every view each frame
            return buildPropertyTree(
                "environment",
                &scene.Environment(),
                *GetDesc<EnvironmentSnapshot>(),
                [] {}
            );
        }

        Widget debugSection() {
            // OnRecord reads it every frame; no dirty consumer
            return buildPropertyTree(
                "debug",
                &Debug(),
                *GetDesc<RenderDebug>(),
                [] {}
            );
        }

        Widget bookmarksSection() {
            return buildPropertyTree(
                "bookmarks",
                &bookmarks,
                *GetDesc<CameraBookmarks>(),
                [this] { snapCamera(); }
            );
        }

        // a snap changes the camera section too, so the panel rebuilds; from
        // inside the panel's own submit this only raises the flag
        void snapCamera() {
            const auto pose = poseOf(bookmarks.current);
            if(!pose)
                return;

            auto& camera = static_cast<FlyCamera&>(Camera());
            camera.position = pose->position;
            camera.yaw = pose->yaw;
            camera.pitch = pose->pitch;
            camera.RecomputeView();
            uiContext.panelDirty = true;
        }

        // each close-up frames its station at eye height
        static std::optional<CameraPose> poseOf(CameraBookmark bookmark) {
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

        Widget cameraSection() {
            auto& camera = static_cast<FlyCamera&>(Camera());

            return buildPropertyTree(
                "camera",
                &camera,
                *GetDesc<FlyCamera>(),
                [&camera] { camera.RecomputeView(); }
            );
        }

        static Config makeConfig() {
            return Config{
                .clearColor = SkyRadiance,
                .drawCapacity = 64,
                .materialCapacity = 64,
                .vertexPoolCapacity = 4096,
                .indexPoolCapacity = 16384
            };
        }

        // the start pose is the overview bookmark, which `current` starts on
        static FlyCamera::Config makeCamera() {
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

        static MaterialHandle addMaterial(
            RenderScene& scene,
            const MaterialData& data,
            const MaterialPipelineDesc& pipeline
        ) {
            return scene.Materials().Add(
                MaterialResource{.data = data, .pipeline = pipeline}
            );
        }

        static MeshHandle addMesh(
            RenderScene& scene,
            const GeometryAllocation& geometry,
            MaterialHandle material,
            Vec3 halfScale
        ) {
            const auto bounds =
                AABB3D{.center = zeros(), .halfScale = halfScale};

            return scene.Meshes().Add(
                MeshResource{
                    .subMeshes =
                        {SubMesh{.geometry = geometry, .localBounds = bounds}},
                    .materials = {material},
                    .localBounds = bounds
                }
            );
        }

        // translate and scale only, so the world bounds are the local ones
        // scaled per axis
        static void addPrimitive(
            RenderScene& scene,
            MeshHandle mesh,
            Vec3 position,
            Vec3 scale
        ) {
            const auto& local = scene.Meshes().GetRef(mesh).localBounds;

            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = translateMat(position) * scaleMat(scale),
                    .worldBounds =
                        AABB3D{
                            .center = position,
                            .halfScale = local.halfScale * scale
                        },
                    .mesh = mesh
                }
            );
        }

        // both stages from one file, so the prepass and the colour pass run
        // the same vertex shader; every material links the default PBR
        static MaterialPipelineDesc basePipeline(
            CStr file,
            CStr fragmentEntry
        ) {
            return MaterialPipelineDesc{
                .vertexShader = {.path = file, .entryPoint = "vs_main"},
                .fragmentShader = {.path = file, .entryPoint = fragmentEntry},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        // the map's own surface: the standard loop plus the grid
        static MaterialPipelineDesc gridPipeline() {
            return basePipeline(
                "Engine/Render/Sample/Playground.slang",
                "fs_grid"
            );
        }

        static MaterialPipelineDesc opaquePipeline() {
            return basePipeline(StandardForward, "fs_opaque");
        }

        static MaterialPipelineDesc doubleSidedPipeline() {
            auto pipeline = opaquePipeline();
            pipeline.rasterizer.cullMode = RHICullMode::None;

            return pipeline;
        }

        static MaterialPipelineDesc translucentPipeline() {
            auto pipeline = basePipeline(StandardForward, "fs_translucent");
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
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "Playground",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<Playground>(argc, argv, windowConfig);
}
