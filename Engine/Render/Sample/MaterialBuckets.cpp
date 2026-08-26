#include <array>
#include <memory>
#include <numbers>

#include <imgui.h>

#include "FlyCamera.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Log.hpp"
#include "MeshGenerator.hpp"
#include "Object.hpp"
#include "PropertyWalker.hpp"
#include "RenderApp.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    struct UIContext{};

    CROWY_STRUCT(MaterialData)
        .SetProperty("albedo", &MaterialData::albedo)
        .SetProperty("metallic", &MaterialData::metallic)
            .SetUIRange(0.0f, 1.0f)
        .SetProperty("emissive", &MaterialData::emissive)
        .SetProperty("roughness", &MaterialData::roughness)
            .SetUIRange(0.0f, 1.0f)
    CROWY_STRUCT_END(MaterialData)

    CROWY_STRUCT(FlyCamera)
        .SetProperty("position", &FlyCamera::position)
        .SetProperty("yaw", &FlyCamera::yaw)
        .SetProperty("pitch", &FlyCamera::pitch)
            .SetUIRange(-1.55f, 1.55f)
        .SetProperty("fovY", &FlyCamera::config, &FlyCamera::Config::fovY)
            .SetUIRange(0.35f, 2.4f)
    CROWY_STRUCT_END(FlyCamera)

    // Bucket order is discovery order, which is primitive order,
    // so the opaque primitives are registered first.
    // There is no sort key yet; the moment a second pass or
    // a real translucent scene arrives, there has to be one.
    class MaterialBuckets: public RenderApp {
        static constexpr f32 Radius = 0.5f;
        static constexpr Color SkyColor{0.10f, 0.11f, 0.14f, 1.0f};

        static constexpr u32 SphereCount = 3;
        static constexpr u32 PaneCount = 3;

        static constexpr auto PanelToggleKey = KeyCode::P;

        GeometryAllocation sphere{};
        // faces away from the camera, so a back-face-culled pipeline drops it
        GeometryAllocation pane{};
        GeometryAllocation glass{};

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        // hidden by default so the smoke capture matches the panel-less one
        bool panelVisible = false;

    public:
        MaterialBuckets()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(
            RHICommandList& cmdList,
            GeometryPool& pool
        ) override {
            const auto sphereMesh = MakeSphere(Radius, 32, 16);
            const auto paneMesh = MakePlane(unitZ(), unitX(), 1.0f);
            const auto glassMesh = MakePlane(-unitZ(), unitX(), 1.0f);

            sphere = pool.Add(cmdList, sphereMesh.vertices, sphereMesh.indices);
            pane = pool.Add(cmdList, paneMesh.vertices, paneMesh.indices);
            glass = pool.Add(cmdList, glassMesh.vertices, glassMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            const auto opaque =
                addMaterial(scene, {0.85f, 0.55f, 0.30f}, opaquePipeline());
            const auto doubleSided = addMaterial(
                scene,
                {0.35f, 0.70f, 0.45f},
                doubleSidedPipeline()
            );
            const auto translucent = addMaterial(
                scene,
                {0.35f, 0.55f, 0.95f},
                translucentPipeline()
            );

            const auto sphereMesh = addMesh(scene, sphere, opaque, Radius);
            const auto paneMesh = addMesh(scene, pane, doubleSided, 1.0f);
            const auto glassMesh = addMesh(scene, glass, translucent, 1.0f);

            // opaque first, so their bucket is discovered first
            for(u32 i = 0; i < SphereCount; ++i) {
                const auto x = (static_cast<f32>(i) - 1.0f) * 1.6f;

                addPrimitive(scene, sphereMesh, {x, 0.0f, 0.0f}, Radius);
            }
            // its front face points away, so this only appears at all because
            // its pipeline culls nothing
            for(u32 i = 0; i < PaneCount; ++i) {
                const auto x = (static_cast<f32>(i) - 1.0f) * 1.6f;

                addPrimitive(scene, paneMesh, {x, 1.8f, 1.5f}, 1.0f);
            }
            for(u32 i = 0; i < PaneCount; ++i) {
                const auto x = (static_cast<f32>(i) - 1.0f) * 1.6f;

                addPrimitive(scene, glassMesh, {x, 0.0f, -1.2f}, 1.0f);
            }

            panel = Column({
                materialSection(scene, "opaque", opaque),
                materialSection(scene, "double-sided", doubleSided),
                materialSection(scene, "translucent", translucent),
                cameraSection()
            });
        }

        void OnProcessInput(const InputProvider& input) override {
            if(input.IsKeyPressed(PanelToggleKey)) {
                panelVisible = !panelVisible;
            }
        }

        void OnInitUI(
            RHIDevice& device,
            RHIPixelFormat colorFormat,
            RHIPixelFormat depthFormat
        ) override {
            uiRenderer = std::make_unique<UIRenderer>(
                device,
                colorFormat,
                depthFormat
            );

            LOG_INFO("MaterialBuckets", "P toggles the inspector panel");
        }

        std::span<const RHITextureBarrier> OnPrepareUI(
            RHICommandList& cmdList
        ) override {
            if(panelVisible) {
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

        void OnRecordUI(RHICommandList& cmdList) override {
            uiRenderer->Record(cmdList);
        }

    private:
        // rows do not move after extraction, so the row's address holds as
        // the panel's write-back target
        Widget materialSection(
            RenderScene& scene,
            CStr label,
            MaterialHandle handle
        ) {
            // BuildFrame re-reads the table every frame; no dirty consumer
            return buildPropertyTree(
                label,
                &scene.Materials().GetRef(handle).data,
                *GetDesc<MaterialData>(),
                []{}
            );
        }

        Widget cameraSection() {
            auto& camera = static_cast<FlyCamera&>(Camera());

            return buildPropertyTree(
                "camera",
                &camera,
                *GetDesc<FlyCamera>(),
                [&camera]{ camera.RecomputeView(); }
            );
        }

        static Config makeConfig() {
            return Config{
                .clearColor = SkyColor,
                .drawCapacity = 32,
                .materialCapacity = 8,
                .vertexPoolCapacity = 4096,
                .indexPoolCapacity = 16384
            };
        }

        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.0f, 0.6f, -5.0f},
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 50.0f
            };
        }

        static MaterialHandle addMaterial(
            RenderScene& scene,
            Vec3 albedo,
            const MaterialPipelineDesc& pipeline
        ) {
            return scene.Materials().Add(
                MaterialResource{
                    .data = MaterialData{.albedo = albedo},
                    .pipeline = pipeline
                }
            );
        }

        static MeshHandle addMesh(
            RenderScene& scene,
            const GeometryAllocation& geometry,
            MaterialHandle material,
            f32 halfSize
        ) {
            const auto bounds =
                AABB3D{.center = zeros(), .halfScale = halfSize * ones()};

            return scene.Meshes().Add(
                MeshResource{
                    .subMeshes =
                        {SubMesh{.geometry = geometry, .localBounds = bounds}},
                    .materials = {material},
                    .localBounds = bounds
                }
            );
        }

        static void addPrimitive(
            RenderScene& scene,
            MeshHandle mesh,
            Vec3 position,
            f32 halfSize
        ) {
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = translateMat(position),
                    .worldBounds =
                        AABB3D{
                            .center = position,
                            .halfScale = halfSize * ones()
                        },
                    .mesh = mesh
                }
            );
        }

        static MaterialPipelineDesc basePipeline(CStr fragmentEntry) {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = "Engine/Shader/MaterialBuckets.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = "Engine/Shader/MaterialBuckets.slang",
                     .entryPoint = fragmentEntry},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
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

int main(void) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "MaterialBuckets",
        .width = 800,
        .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<MaterialBuckets>(windowConfig);
}
