#include <array>
#include <format>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <imgui.h>

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
    struct UIContext {};

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
        // Bucket order is discovery order, which is primitive order, so the
        // primitives go grid, opaque, double-sided, translucent last.
        // There is no sort key yet; the moment a second pass or
        // a real translucent scene arrives, there has to be one.
        class Playground: public RenderApp {
        // the line every station's front stands on
        static constexpr f32 FrontZ = 3.5f;

        // dielectric in front, metal behind: a real surface is one or the
        // other, so metallic takes no values between
        static constexpr u32 ChartRows = 2;
        // roughness 0, 0.25, 0.5, 0.75, 1 from left to right
        static constexpr u32 ChartColumns = 5;
        static constexpr f32 SphereRadius = 0.25f;
        // one colour for both rows, so metallic is all that differs;
        // MaterialBuckets' orange, which a metal's highlight takes
        static constexpr Vec3 ChartAlbedo{0.85f, 0.55f, 0.30f};

        // Godot's editor-preview sky
        static constexpr Color SkyColor{0.385f, 0.454f, 0.55f, 1.0f};

        static constexpr auto PanelToggleKey = KeyCode::P;
        static constexpr auto StatsToggleKey = KeyCode::I;

        struct NamedMaterial {
            CStr name;
            MaterialHandle handle;
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
        // hidden by default so the smoke capture matches the panel-less one
        bool panelVisible = false;

        StatsOverlay statsOverlay;
        // hidden by default, for the same reason as the panel
        bool statsVisible = false;

    public:
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
            // near black, so the emissive panels read against it: there are
            // no shadows to darken a niche with
            const auto boardMaterial = addMaterial(
                scene,
                MaterialData{
                    .albedo = {0.03f, 0.03f, 0.03f},
                    .roughness = 0.9f
                },
                opaquePipeline()
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
                MaterialData{.albedo = {0.35f, 0.55f, 0.95f}},
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
                {0.0f, 2.0f, 10.1f},
                {20.4f, 4.0f, 0.2f}
            );
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

            // opaque. emissive: three 0.5 m panels on a board 2.5 m wide
            addPrimitive(
                scene,
                addMesh(scene, unitBox, boardMaterial, boxHalf),
                {-4.25f, 1.0f, FrontZ + 0.125f},
                {2.5f, 2.0f, 0.25f}
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
                    {x, 1.25f, FrontZ - 0.01f},
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

            constexpr auto Last = ChartColumns - 1;
            const std::array exposed{
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

            std::vector<Widget> sections{cameraSection()};
            for(const auto& material: exposed) {
                sections.push_back(
                    materialSection(scene, material.name, material.handle)
                );
            }
            panel = Column(std::move(sections));

            // the port's targets are the panel's, under the same rule
            if(auto* port = Port()) {
                auto& camera = static_cast<FlyCamera&>(Camera());

                port->Expose(
                    "camera",
                    &camera,
                    *GetDesc<FlyCamera>(),
                    [&camera] { camera.RecomputeView(); }
                );
                for(const auto& material: exposed) {
                    port->Expose(
                        std::format("material.{}", material.name),
                        &materialData(scene, material.handle),
                        *GetDesc<MaterialData>()
                    );
                }
            }
        }

        void OnProcessInput(const InputProvider& input) override {
            if(input.IsKeyPressed(PanelToggleKey)) {
                panelVisible = !panelVisible;
            }
            if(input.IsKeyPressed(StatsToggleKey)) {
                statsVisible = !statsVisible;
            }
        }

        void OnInitUI(
            RHIDevice& device,
            RHIPixelFormat colorFormat,
            RHIPixelFormat depthFormat
        ) override {
            uiRenderer =
                std::make_unique<UIRenderer>(device, colorFormat, depthFormat);

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
            if(statsVisible) {
                statsOverlay.Draw(LastFrameStats());
            }

            if(panelVisible) {
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

        Widget materialSection(
            RenderScene& scene,
            CStr label,
            MaterialHandle handle
        ) {
            // BuildFrame re-reads the table every frame; no dirty consumer
            return buildPropertyTree(
                label,
                &materialData(scene, handle),
                *GetDesc<MaterialData>(),
                [] {}
            );
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
                .clearColor = SkyColor,
                .drawCapacity = 64,
                .materialCapacity = 64,
                .vertexPoolCapacity = 4096,
                .indexPoolCapacity = 16384
            };
        }

        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.0f, 1.7f, -5.0f},
                .pitch = 0.1f,
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

        static MaterialPipelineDesc basePipeline(CStr fragmentEntry) {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = "Engine/Render/Sample/Playground.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = "Engine/Render/Sample/Playground.slang",
                     .entryPoint = fragmentEntry},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        static MaterialPipelineDesc gridPipeline() {
            return basePipeline("fs_grid");
        }

        static MaterialPipelineDesc opaquePipeline() {
            return basePipeline("fs_opaque");
        }

        static MaterialPipelineDesc doubleSidedPipeline() {
            auto pipeline = basePipeline("fs_opaque");
            pipeline.rasterizer.cullMode = RHICullMode::None;

            return pipeline;
        }

        static MaterialPipelineDesc translucentPipeline() {
            auto pipeline = basePipeline("fs_translucent");
            pipeline.depthWrite = false;

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
