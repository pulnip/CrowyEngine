#include "PlaygroundScene.hpp"

#include <format>
#include <memory>
#include <numbers>
#include <utility>

#include <imgui.h>

#include "Log.hpp"
#include "MeshGenerator.hpp"
#include "Object.hpp"
#include "PortStatusChip.hpp"
#include "PropertyWalker.hpp"
#include "ShadingModel.hpp"

namespace Crowy
{
    PlaygroundScene::~PlaygroundScene() {
        if(auto* port = Port()) {
            port->Unexpose("bookmarks");
            port->Unexpose("shading");
            port->Unexpose("camera");
            port->Unexpose("environment");
            for(const auto& light: exposedLights)
                port->Unexpose(std::format("light.{}", light.name));
            for(const auto& material: exposedMaterials)
                port->Unexpose(std::format("material.{}", material.name));
        }
    }

    PlaygroundScene::PlaygroundScene(const Config& config)
        : RenderApp(config, std::make_unique<FlyCamera>(MakeCamera())) {}

    RenderApp::Config PlaygroundScene::MakeConfig() {
        return Config{
            .clearColor = SkyRadiance,
            .drawCapacity = 64,
            .materialCapacity = 64,
            .vertexPoolCapacity = 4096,
            .indexPoolCapacity = 16384
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

    void PlaygroundScene::OnBuildGeometry(GeometryPool& pool) {
        const auto sphereMesh = MakeSphere(SphereRadius, 48, 24);
        const auto boxMesh = MakeBox(0.5f);
        const auto floorMesh = MakePlane(Vec2{10.0f, 10.0f});
        const auto towardMesh = MakePlane(-unitZ(), unitX(), 0.5f);
        const auto awayMesh = MakePlane(unitZ(), unitX(), 0.5f);

        sphere = pool.Add(sphereMesh.vertices, sphereMesh.indices);
        unitBox = pool.Add(boxMesh.vertices, boxMesh.indices);
        floorPlane = pool.Add(floorMesh.vertices, floorMesh.indices);
        quadFacingCamera = pool.Add(towardMesh.vertices, towardMesh.indices);
        quadFacingAway = pool.Add(awayMesh.vertices, awayMesh.indices);
    }

    void PlaygroundScene::ExtractScene(RenderScene& scene) {
        constexpr auto ChartRows = PlaygroundChartRows;
        constexpr auto ChartColumns = PlaygroundChartColumns;
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

        materials.floor = addMaterial(
            scene,
            MaterialData{.albedo = {0.5f, 0.5f, 0.5f}, .roughness = 0.8f},
            gridPipeline()
        );
        materials.wall = addMaterial(
            scene,
            MaterialData{.albedo = {0.62f, 0.62f, 0.6f}, .roughness = 0.8f},
            gridPipeline()
        );
        // the scale blocks and the chart's step
        materials.block = addMaterial(
            scene,
            MaterialData{.albedo = {0.55f, 0.62f, 0.72f}, .roughness = 0.6f},
            gridPipeline()
        );
        // 1, 4 and 16, so the tone mapper's compression shows
        for(u32 i = 0; i < materials.emissive.size(); ++i) {
            const auto radiance = static_cast<f32>(1u << (2 * i));

            materials.emissive[i] = addMaterial(
                scene,
                MaterialData{.albedo = zeros(), .emissive = radiance * ones()},
                opaquePipeline()
            );
        }

        // the lanes carry Toon's defaults, which PBR ignores, so a switch
        // to Toon never meets a zero threshold or a black tint
        constexpr ToonLanes ChartLanes{};
        for(u32 row = 0; row < ChartRows; ++row) {
            for(u32 column = 0; column < ChartColumns; ++column) {
                constexpr auto Step = 1.0f / (ChartColumns - 1);

                materials.chart[row * ChartColumns + column] = addMaterial(
                    scene,
                    MaterialData{
                        .albedo = ChartAlbedo,
                        .metallic = static_cast<f32>(row),
                        .roughness = static_cast<f32>(column) * Step,
                        .custom0 = toonCustom0(ChartLanes),
                        .custom1 = toonCustom1(ChartLanes)
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
        for(u32 i = 0; i < materials.bars.size(); ++i) {
            materials.bars[i] = addMaterial(
                scene,
                MaterialData{.albedo = barColors[i]},
                opaquePipeline()
            );
        }

        materials.doubleSided = addMaterial(
            scene,
            MaterialData{.albedo = {0.35f, 0.70f, 0.45f}},
            doubleSidedPipeline()
        );
        materials.translucent = addMaterial(
            scene,
            MaterialData{.albedo = {0.35f, 0.55f, 0.95f}, .opacity = 0.45f},
            translucentPipeline()
        );

        OnRestyleMaterials(scene, materials, shading);

        // grid: the floor, three walls leaving the camera side open, the
        // scale blocks and the chart's step
        const auto blockMesh =
            addMesh(scene, unitBox, materials.block, boxHalf);
        addPrimitive(
            scene,
            addMesh(scene, floorPlane, materials.floor, {10.0f, 0.0f, 10.0f}),
            zeros(),
            ones()
        );
        const auto wallMesh = addMesh(scene, unitBox, materials.wall, boxHalf);
        addPrimitive(
            scene,
            wallMesh,
            {-10.1f, 2.0f, 0.0f},
            {0.2f, 4.0f, 20.4f}
        );
        addPrimitive(scene, wallMesh, {10.1f, 2.0f, 0.0f}, {0.2f, 4.0f, 20.4f});
        // nearer centres draw first, so the back wall draws after the side
        // walls, and Equal behind the prepass keeps it on the corner seams
        addPrimitive(scene, wallMesh, {0.0f, 2.0f, 10.1f}, {20.4f, 4.0f, 0.2f});

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
        addPrimitive(scene, blockMesh, {0.0f, 0.5f, 4.25f}, {5.0f, 1.0f, 0.5f});

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
        for(u32 i = 0; i < materials.emissive.size(); ++i) {
            const auto x = -5.0f + 0.75f * static_cast<f32>(i);

            addPrimitive(
                scene,
                addMesh(
                    scene,
                    quadFacingCamera,
                    materials.emissive[i],
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
                    materials.chart[row * ChartColumns + column];
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
        for(u32 i = 0; i < materials.bars.size(); ++i) {
            addPrimitive(
                scene,
                addMesh(scene, unitBox, materials.bars[i], boxHalf),
                {3.5f + 0.75f * static_cast<f32>(i), 1.0f, FrontZ + 1.125f},
                {0.5f, 2.0f, 0.25f}
            );
        }

        // double-sided: a pane facing the camera beside one showing its
        // back, which shows only because nothing is culled; the two
        // should look alike
        addPrimitive(
            scene,
            addMesh(scene, quadFacingCamera, materials.doubleSided, QuadHalf),
            {6.5f, 0.75f, FrontZ},
            {1.0f, 1.5f, 1.0f}
        );
        addPrimitive(
            scene,
            addMesh(scene, quadFacingAway, materials.doubleSided, QuadHalf),
            {7.75f, 0.75f, FrontZ},
            {1.0f, 1.5f, 1.0f}
        );

        // translucent, last: the pane in front of the bars
        addPrimitive(
            scene,
            addMesh(scene, quadFacingCamera, materials.translucent, QuadHalf),
            {4.25f, 0.75f, FrontZ},
            {2.5f, 1.5f, 1.0f}
        );

        exposedLights = {NamedLight{"sun", sun}, NamedLight{"lamp", lamp}};

        constexpr auto Last = ChartColumns - 1;
        exposedMaterials = {
            NamedMaterial{"floor", materials.floor},
            NamedMaterial{"smooth-dielectric", materials.chart[0]},
            NamedMaterial{"rough-dielectric", materials.chart[Last]},
            NamedMaterial{"smooth-metal", materials.chart[ChartColumns]},
            NamedMaterial{"rough-metal", materials.chart[ChartColumns + Last]},
            NamedMaterial{"double-sided", materials.doubleSided},
            NamedMaterial{"translucent", materials.translucent},
            // the middle panel, at 4
            NamedMaterial{"emissive", materials.emissive[1]}
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
                "shading",
                &shading,
                *GetDesc<PlaygroundShading>(),
                [this] {
                    linkChartShading();
                    uiContext.panelDirty = true;
                }
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

    void PlaygroundScene::OnProcessInput(const InputProvider& input) {
        auto& debug = Debug();
        if(input.IsKeyPressed(PanelToggleKey)) {
            debug.showPanel = !debug.showPanel;
        }
        if(input.IsKeyPressed(StatsToggleKey)) {
            debug.showStats = !debug.showStats;
        }
    }

    void PlaygroundScene::OnInitUI(
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

    std::span<const RHITextureBarrier> PlaygroundScene::OnPrepareUI(
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
        } else {
            uiRenderer->Prepare(cmdList);
        }

        return uiRenderer->TextureAcquires();
    }

    void PlaygroundScene::OnRecordUI(RHICommandList& cmdList) {
        uiRenderer->Record(cmdList);
    }

    MaterialData& PlaygroundScene::materialData(
        RenderScene& scene,
        MaterialHandle handle
    ) {
        return scene.Materials().GetRef(handle).data;
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

    MaterialHandle PlaygroundScene::addMaterial(
        RenderScene& scene,
        const MaterialData& data,
        const MaterialPipelineDesc& pipeline
    ) {
        return scene.Materials().Add(
            MaterialResource{.data = data, .pipeline = pipeline}
        );
    }

    MeshHandle PlaygroundScene::addMesh(
        RenderScene& scene,
        const GeometryAllocation& geometry,
        MaterialHandle material,
        Vec3 halfScale
    ) {
        const auto bounds = AABB3D{.center = zeros(), .halfScale = halfScale};

        return scene.Meshes().Add(
            MeshResource{
                .subMeshes =
                    {SubMesh{.geometry = geometry, .localBounds = bounds}},
                .materials = {material},
                .localBounds = bounds
            }
        );
    }

    void PlaygroundScene::addPrimitive(
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

    MaterialPipelineDesc PlaygroundScene::basePipeline(
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

    MaterialPipelineDesc PlaygroundScene::gridPipeline() {
        return basePipeline("Engine/Render/Sample/Playground.slang", "fs_grid");
    }

    MaterialPipelineDesc PlaygroundScene::opaquePipeline() {
        return basePipeline(StandardForward, "fs_opaque");
    }

    MaterialPipelineDesc PlaygroundScene::doubleSidedPipeline() {
        auto pipeline = opaquePipeline();
        pipeline.rasterizer.cullMode = RHICullMode::None;

        return pipeline;
    }

    MaterialPipelineDesc PlaygroundScene::translucentPipeline() {
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

    Widget PlaygroundScene::buildPanel() {
        std::vector<Widget> sections{
            debugSection(),
            shadingSection(),
            bookmarksSection(),
            cameraSection()
        };
        for(const auto& light: exposedLights) {
            sections.push_back(lightSection(Scene(), light.name, light.handle));
        }
        sections.push_back(environmentSection(Scene()));
        for(const auto& material: exposedMaterials) {
            sections.push_back(
                materialSection(Scene(), material.name, material.handle)
            );
        }

        return Column(std::move(sections));
    }

    Widget PlaygroundScene::materialSection(
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

    Widget PlaygroundScene::lightSection(
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

    Widget PlaygroundScene::environmentSection(RenderScene& scene) {
        // BeginFrame copies it into every view each frame
        return buildPropertyTree(
            "environment",
            &scene.Environment(),
            *GetDesc<EnvironmentSnapshot>(),
            [] {}
        );
    }

    Widget PlaygroundScene::debugSection() {
        // OnRecord reads it every frame; no dirty consumer
        return buildPropertyTree(
            "debug",
            &Debug(),
            *GetDesc<RenderDebug>(),
            [] {}
        );
    }

    Widget PlaygroundScene::shadingSection() {
        // the draw lists resolve every material's pipeline each frame,
        // so the next frame links the new model
        return buildPropertyTree(
            "shading",
            &shading,
            *GetDesc<PlaygroundShading>(),
            [this] { linkChartShading(); }
        );
    }

    Widget PlaygroundScene::bookmarksSection() {
        return buildPropertyTree(
            "bookmarks",
            &bookmarks,
            *GetDesc<CameraBookmarks>(),
            [this] { snapCamera(); }
        );
    }

    Widget PlaygroundScene::cameraSection() {
        auto& camera = static_cast<FlyCamera&>(Camera());

        return buildPropertyTree(
            "camera",
            &camera,
            *GetDesc<FlyCamera>(),
            [&camera] { camera.RecomputeView(); }
        );
    }

    void PlaygroundScene::linkChartShading() {
        for(const auto handle: materials.chart) {
            Scene().Materials().GetRef(handle).pipeline.shadingModule =
                moduleOf(shading.chart);
        }
    }

    void PlaygroundScene::snapCamera() {
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

    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(CameraBookmarks)
        .SetProperty("current", &CameraBookmarks::current)
    CROWY_STRUCT_END(CameraBookmarks)

    CROWY_STRUCT(PlaygroundShading)
        .SetProperty("chart", &PlaygroundShading::chart)
    CROWY_STRUCT_END(PlaygroundShading)

    // clang-format on
}
