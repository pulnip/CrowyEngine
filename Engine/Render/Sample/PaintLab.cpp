#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <imgui.h>

#include "FlyCamera.hpp"
#include "Geometry/Overlap3D.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Log.hpp"
#include "MeshData.hpp"
#include "MintFrame.hpp"
#include "Object.hpp"
#include "PaintStage.hpp"
#include "PortStatusChip.hpp"
#include "PropertyWalker.hpp"
#include "RenderApp.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    struct UIContext {
        bool panelDirty = false;
    };

    // MintChoco's paint, rebuilt one step at a time on a stage of blocks
    class PaintLab final: public RenderApp {
        // ML_Look_Plain, the shipped blocks' bare surface
        static constexpr Vec3 PlainAlbedo{0.72f, 0.70f, 0.66f};
        static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};
        static constexpr CStr StandardForward =
            "Engine/Render/Shader/StandardForward.slang";
        static constexpr auto PanelToggleKey = KeyCode::P;

        PaintStageMeshes meshes = makePaintStageMeshes();
        std::vector<PaintStageObject> objects = makePaintStageObjects();
        std::array<GeometryAllocation, PaintMeshKindCount> geometry{};
        std::vector<PrimitiveHandle> primitives;
        PaintStageKind stage = PaintStageKind::Block;

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        RenderDebug shownDebug;

    public:
        ~PaintLab() override {
            if(auto* port = Port())
                port->Unexpose("camera");
        }
        CROWY_DECLARE_PINNED(PaintLab)

        PaintLab()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            for(usize kind = 0; kind < PaintMeshKindCount; ++kind) {
                const auto data =
                    toMeshData(meshes.Get(static_cast<PaintMeshKind>(kind)));
                geometry[kind] = pool.Add(data.vertices, data.indices);
            }
        }

        void ExtractScene(RenderScene& scene) override {
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

            const auto plain = scene.Materials().Add(
                MaterialResource{
                    .data = {.albedo = PlainAlbedo, .roughness = 0.6f},
                    .pipeline = opaquePipeline()
                }
            );
            for(const auto& object: objects) {
                const auto local = localBounds(object.mesh);
                const auto mesh = scene.Meshes().Add(
                    MeshResource{
                        .subMeshes = {SubMesh{
                            .geometry =
                                geometry[static_cast<usize>(object.mesh)],
                            .localBounds = local
                        }},
                        .materials = {plain},
                        .localBounds = local
                    }
                );
                const auto localToWorld =
                    mintToCrowy() * object.transform.ToMat4();
                primitives.push_back(scene.Primitives().Add(
                    PrimitiveSnapshot{
                        .localToWorld = localToWorld,
                        .worldBounds = transformAABB3D(localToWorld, local),
                        .mesh = mesh,
                        .flags = flagsFor(object)
                    }
                ));
            }

            shownDebug = Debug();
            panel = buildPanel();

            if(auto* port = Port()) {
                auto& camera = static_cast<FlyCamera&>(Camera());
                port->Expose(
                    "camera",
                    &camera,
                    *GetDesc<FlyCamera>(),
                    [this, &camera] {
                        camera.RecomputeView();
                        uiContext.panelDirty = true;
                    }
                );
            }
        }

        void OnProcessInput(const InputProvider& input) override {
            if(input.IsKeyPressed(PanelToggleKey))
                Debug().showPanel = !Debug().showPanel;
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

            LOG_INFO("PaintLab", "P toggles the panel");
        }

        std::span<const RHITextureBarrier> OnPrepareUI(
            RHICommandList& cmdList
        ) override {
            const auto& debug = Debug();
            if(debug != shownDebug) {
                shownDebug = debug;
                uiContext.panelDirty = true;
            }
            if(std::exchange(uiContext.panelDirty, false))
                panel = buildPanel();

            // the chrome stays out of every capture unless asked for
            if(debug.showPanel) {
                if(auto* port = Port())
                    drawPortStatusChip(port->Status());

                ImGui::SetNextWindowPos(
                    ImVec2(8.0f, 8.0f),
                    ImGuiCond_Appearing
                );
                ImGui::SetNextWindowSize(
                    ImVec2(380.0f, 720.0f),
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
        static Config makeConfig() {
            return Config{
                .clearColor = SkyRadiance,
                .drawCapacity = 64,
                .materialCapacity = 64,
                .shadowMapSize = 2048,
                .vertexPoolCapacity = 16384,
                .indexPoolCapacity = 65536
            };
        }

        // the stage from its south-west corner, a storey up
        static FlyCamera::Config makeCamera() {
            const auto eye = toCrowyPoint({-1300.0, -1500.0, 900.0});
            const auto target = toCrowyPoint({0.0, 0.0, 100.0});
            const auto forward = normalize(target - eye);

            return FlyCamera::Config{
                .position = eye,
                .yaw = std::atan2(forward.x, forward.z),
                .pitch = std::asin(-forward.y),
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 200.0f,
                .moveSpeed = 8.0f
            };
        }

        static MaterialPipelineDesc opaquePipeline() {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = StandardForward, .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = StandardForward, .entryPoint = "fs_opaque"},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        // the pool keeps Mint-local cm; each primitive's matrix takes it out
        static MeshData toMeshData(const PaintMeshTriangles& mesh) {
            MeshData data;
            data.vertices.reserve(mesh.positions.size());
            for(usize i = 0; i < mesh.positions.size(); ++i) {
                data.vertices.push_back(
                    Vertex{
                        .position = mesh.positions[i],
                        .normal = mesh.normals[i],
                        .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
                    }
                );
            }
            data.indices = mesh.indices;

            return data;
        }

        AABB3D localBounds(PaintMeshKind kind) const {
            const auto bounds = boundsOf(meshes.Get(kind));

            return AABB3D{
                .center = toVec3(bounds.Center()),
                .halfScale = toVec3(bounds.Extent())
            };
        }

        PrimitiveFlags flagsFor(const PaintStageObject& object) const {
            return object.stage == stage ? combine(
                                               PrimitiveFlags::Visible,
                                               PrimitiveFlags::CastShadow
                                           )
                                         : PrimitiveFlags::None;
        }

        Widget buildPanel() {
            auto& camera = static_cast<FlyCamera&>(Camera());

            return Column({
                buildPropertyTree(
                    "debug",
                    &Debug(),
                    *GetDesc<RenderDebug>(),
                    [] {}
                ),
                buildPropertyTree(
                    "camera",
                    &camera,
                    *GetDesc<FlyCamera>(),
                    [&camera] { camera.RecomputeView(); }
                ),
            });
        }
    };

    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(FlyCamera)
        .SetProperty("position", &FlyCamera::position)
        .SetProperty("yaw", &FlyCamera::yaw)
        .SetProperty("pitch", &FlyCamera::pitch)
        .SetUIRange(-1.55f, 1.55f)
        .SetProperty("fovY", &FlyCamera::config, &FlyCamera::Config::fovY)
        .SetUIRange(0.35f, 2.4f)
    CROWY_STRUCT_END(FlyCamera)
    // clang-format on
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "PaintLab",
        .width = 1920,
        .height = 1080,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<PaintLab>(argc, argv, windowConfig);
}
