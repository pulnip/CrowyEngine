#include <algorithm>
#include <array>
#include <cmath>
#include <format>
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
#include "PaintLabControl.hpp"
#include "PaintLabGpu.hpp"
#include "PaintStage.hpp"
#include "PaintSurface.hpp"
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
        static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};
        static constexpr CStr StandardForward =
            "Engine/Render/Shader/StandardForward.slang";
        static constexpr CStr PaintHook = "paint";
        static constexpr CStr PanelHook = "paintPanels";
        static constexpr auto PanelToggleKey = KeyCode::P;
        static constexpr f32 PanelSize = 640.0f;
        static constexpr f32 PanelMargin = 24.0f;
        static constexpr std::array<CStr, PaintFaceDirectionCount>
            DirectionNames{"Front", "Back", "Right", "Left", "Up", "Down"};

        PaintStageMeshes meshes = makePaintStageMeshes();
        std::vector<PaintSurface> surfaces;
        // one per surface, in the same order: its flags as the port sees them
        std::vector<PaintObjectFlags> flags;
        // 1 where the surface is on the stage being shown
        std::vector<u8> active;
        std::array<GeometryAllocation, PaintMeshKindCount> geometry{};
        std::vector<PrimitiveHandle> proxies;
        RAII<PaintGpu> gpu;
        std::vector<PaintGpuStamp> frameStamps;
        PaintLabSettings settings;
        PaintStageChoice shownStage = PaintStageChoice::Block;
        u64 recordedFrames = 0;

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        RenderDebug shownDebug;

    public:
        ~PaintLab() override {
            if(auto* port = Port()) {
                port->Unexpose("camera");
                port->Unexpose("lab");
                for(const auto& surface: surfaces)
                    port->Unexpose(
                        std::format("stage.{}", surface.Object().name)
                    );
            }
        }
        CROWY_DECLARE_PINNED(PaintLab)

        PaintLab()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {
            for(auto& object: makePaintStageObjects()) {
                flags.push_back(flagsOf(object));
                const auto& mesh = meshes.Get(object.mesh);
                surfaces.emplace_back(std::move(object), mesh);
            }
            refreshActive();
        }

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

            // the surfaces draw in the paint hook; these only cast their
            // shadows, Masked so the opaque passes skip them
            const auto shadowOnly = scene.Materials().Add(
                MaterialResource{
                    .data =
                        {.albedo = {0.72f, 0.70f, 0.66f}, .roughness = 0.6f},
                    .pipeline = proxyPipeline()
                }
            );
            for(usize i = 0; i < surfaces.size(); ++i) {
                const auto& object = surfaces[i].Object();
                const auto local = localBounds(object.mesh);
                const auto mesh = scene.Meshes().Add(
                    MeshResource{
                        .subMeshes = {SubMesh{
                            .geometry =
                                geometry[static_cast<usize>(object.mesh)],
                            .localBounds = local
                        }},
                        .materials = {shadowOnly},
                        .localBounds = local
                    }
                );
                const auto localToWorld =
                    mintToCrowy() * object.transform.ToMat4();
                proxies.push_back(scene.Primitives().Add(
                    PrimitiveSnapshot{
                        .localToWorld = localToWorld,
                        .worldBounds = transformAABB3D(localToWorld, local),
                        .mesh = mesh,
                        .flags = proxyFlags(i)
                    }
                ));
            }

            gpu = std::make_unique<PaintGpu>(Device());
            shownDebug = Debug();
            panel = buildPanel();
            exposeToPort();
        }

        FramePipelineDesc DescribePipeline(
            const StandardPipelineConfig& config
        ) override {
            auto desc = makeStandardPipeline(config);
            // a data view is the standard list, and the proxies draw plainly
            if(config.post != std::vector<PostPassDesc>{tonemapPass()})
                return desc;

            for(auto& pass: desc.passes) {
                if(pass.name != "DepthPrepass" && pass.name != "Opaque")
                    continue;
                if(auto* mesh = std::get_if<MeshPassDesc>(&pass.kind))
                    mesh->filter.domains = MaterialDomain::Opaque;
            }

            const auto translucent =
                std::ranges::find(desc.passes, "Translucent", &PassDesc::name);
            const auto depth = std::ranges::find(
                desc.targets,
                "SceneDepth",
                &FrameTargetDesc::name
            );
            if(translucent == desc.passes.end() || depth == desc.targets.end())
                return desc;

            const auto sceneDepth =
                static_cast<FrameTargetID>(depth - desc.targets.begin() + 1);
            std::vector<FrameTargetID> reads;
            if(desc.shadowMap != 0)
                reads.push_back(desc.shadowMap);
            desc.passes.insert(
                translucent,
                PassDesc{
                    .name = "Paint",
                    .colors = {ColorTargetUse{
                        .target = desc.sceneColor,
                        .load = RHILoadAction::Load
                    }},
                    .depth =
                        DepthTargetUse{
                            .target = sceneDepth,
                            .load = RHILoadAction::Load
                        },
                    .reads = reads,
                    .kind = HookPassDesc{.hook = PaintHook}
                }
            );
            // after the post chain, on the back buffer's display values; the
            // UI rides this pass, so it keeps one colour target and no depth
            desc.passes.push_back(
                PassDesc{
                    .name = "PaintPanels",
                    .colors = {ColorTargetUse{
                        .target = BackBufferTarget,
                        .load = RHILoadAction::Load
                    }},
                    .kind = HookPassDesc{.hook = PanelHook}
                }
            );

            return desc;
        }

        void OnProcessInput(const InputProvider& input) override {
            if(input.IsKeyPressed(PanelToggleKey))
                Debug().showPanel = !Debug().showPanel;
        }

        void OnUpdateScene(f64) override {
            if(settings.stage != shownStage) {
                shownStage = settings.stage;
                refreshActive();
                for(usize i = 0; i < proxies.size(); ++i)
                    Scene().Primitives().GetRef(proxies[i]).flags =
                        proxyFlags(i);
            }
        }

        std::vector<PassHook> OnRecordSimulation(
            RHICommandList& cmdList
        ) override {
            ++recordedFrames;
            gpu->Sync(surfaces);
            const auto acquires = gpu->Record(
                cmdList,
                Renderer().Pipelines(),
                surfaces,
                frameStamps
            );
            frameStamps.clear();

            std::vector<PassHook> hooks;
            if(FindHook(PaintHook)) {
                hooks.push_back(
                    PassHook{
                        .name = PaintHook,
                        .textureAcquires = acquires,
                        .record = [this](
                                      RHICommandList& cmdList,
                                      const HookPassContext& context
                                  ) { return drawSurfaces(cmdList, context); }
                    }
                );
            }
            if(FindHook(PanelHook)) {
                hooks.push_back(
                    PassHook{
                        .name = PanelHook,
                        .textureAcquires = acquires,
                        .record = [this](
                                      RHICommandList& cmdList,
                                      const HookPassContext& context
                                  ) { return drawPanels(cmdList, context); }
                    }
                );
            }

            return hooks;
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

            drawPanelLabels();

            // the chrome stays out of every capture unless asked for
            if(debug.showPanel) {
                if(auto* port = Port())
                    drawPortStatusChip(port->Status());

                ImGui::SetNextWindowPos(
                    ImVec2(8.0f, 8.0f),
                    ImGuiCond_Appearing
                );
                ImGui::SetNextWindowSize(
                    ImVec2(380.0f, 760.0f),
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

        // Masked, so the narrowed opaque passes leave them to the shadow map
        static MaterialPipelineDesc proxyPipeline() {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = StandardForward, .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = StandardForward, .entryPoint = "fs_masked"},
                .maskShader =
                    {.path = StandardForward, .entryPoint = "fs_masked_depth"},
                .rasterizer = {.frontCounterClockwise = false},
                .domain = MaterialDomain::Masked,
                .profile = "sm_6_8"
            };
        }

        // the pool keeps Mint-local cm; each matrix takes it out
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

        static PaintObjectFlags flagsOf(const PaintStageObject& object) {
            using enum PaintFaceDirection;
            const auto has = [&](PaintFaceDirection d) {
                return (object.directions & paintDirectionBit(d)) != 0;
            };

            return PaintObjectFlags{
                .front = has(Front),
                .back = has(Back),
                .right = has(Right),
                .left = has(Left),
                .up = has(Up),
                .down = has(Down),
                .floorFollowsWorldUp = object.floorFollowsWorldUp
            };
        }

        static u8 maskOf(const PaintObjectFlags& f) {
            using enum PaintFaceDirection;
            u8 mask = 0;
            const std::array<std::pair<bool, PaintFaceDirection>, 6> bits{
                {{f.front, Front},
                 {f.back, Back},
                 {f.right, Right},
                 {f.left, Left},
                 {f.up, Up},
                 {f.down, Down}}
            };
            for(const auto& [on, d]: bits) {
                if(on)
                    mask |= paintDirectionBit(d);
            }

            return mask;
        }

        PaintStageKind stageKind() const noexcept {
            return settings.stage == PaintStageChoice::Block
                       ? PaintStageKind::Block
                       : PaintStageKind::Grid;
        }

        void refreshActive() {
            active.assign(surfaces.size(), 0);
            for(usize i = 0; i < surfaces.size(); ++i)
                active[i] = surfaces[i].Object().stage == stageKind() ? 1 : 0;
        }

        PrimitiveFlags proxyFlags(usize i) const {
            return active[i] != 0 ? combine(
                                        PrimitiveFlags::Visible,
                                        PrimitiveFlags::CastShadow
                                    )
                                  : PrimitiveFlags::None;
        }

        AABB3D localBounds(PaintMeshKind kind) const {
            const auto bounds = boundsOf(meshes.Get(kind));

            return AABB3D{
                .center = toVec3(bounds.Center()),
                .halfScale = toVec3(bounds.Extent())
            };
        }

        Vec4 panelRect() const {
            const auto width = static_cast<f32>(1920);

            return Vec4{
                width - PanelSize - PanelMargin,
                PanelMargin,
                PanelSize,
                PanelSize
            };
        }

        usize selectedSurface() const {
            return static_cast<usize>(std::clamp<i32>(
                settings.selected,
                0,
                static_cast<i32>(surfaces.size()) - 1
            ));
        }

        u32 drawSurfaces(
            RHICommandList& cmdList,
            const HookPassContext& context
        ) {
            // the pool's first upload rides the opaque passes' acquires; the
            // first frame draws nothing here rather than read past them
            if(recordedFrames < 2)
                return 0;

            auto push = Renderer().FramePush();
            push.vertices = Geometry().GetVertexBufferID();
            push.shadowMap = context.reads.empty() ? 0 : context.reads[0];
            const auto split = std::clamp(settings.split, 0.0f, 1.0f);

            return gpu->DrawSurfaces(
                cmdList,
                context,
                Renderer().Pipelines(),
                push,
                RHIIndexBufferView{.buffer = &Geometry().GetIndexBuffer()},
                surfaces,
                active,
                geometry,
                PaintDrawSettings{
                    .view = static_cast<u32>(settings.view),
                    .compareView = static_cast<u32>(settings.compareView),
                    .splitPixels = split >= 1.0f ? 1e9f : split * 1920.0f
                }
            );
        }

        u32 drawPanels(
            RHICommandList& cmdList,
            const HookPassContext& context
        ) {
            if(settings.panel == PaintPanel::None || recordedFrames < 2)
                return 0;
            const auto index = selectedSurface();

            return gpu->DrawPanel(
                cmdList,
                context,
                Renderer().Pipelines(),
                surfaces[index],
                index,
                static_cast<u32>(settings.panelChannel),
                panelRect()
            );
        }

        // the panel's island rectangles and names, and what the layout is
        void drawPanelLabels() {
            if(settings.panel == PaintPanel::None || !settings.labels)
                return;
            const auto& surface = surfaces[selectedSurface()];
            const auto& layout = surface.Layout();
            if(layout.atlasSize == 0)
                return;

            auto* draw = ImGui::GetForegroundDrawList();
            const auto rect = panelRect();
            const auto scale = rect.z / static_cast<f32>(layout.atlasSize);
            const auto white = IM_COL32(255, 255, 255, 230);
            draw->AddRect(
                ImVec2(rect.x - 1.0f, rect.y - 1.0f),
                ImVec2(rect.x + rect.z + 1.0f, rect.y + rect.w + 1.0f),
                white
            );
            for(const auto& island: layout.islands) {
                const ImVec2 from{
                    rect.x + island.rect.min.x * scale,
                    rect.y + island.rect.min.y * scale
                };
                const ImVec2 to{
                    rect.x + island.rect.max.x * scale,
                    rect.y + island.rect.max.y * scale
                };
                draw->AddRect(from, to, white, 0.0f, 0, 2.0f);
                const auto label = std::format(
                    "{} {}x{}",
                    DirectionNames[static_cast<usize>(island.direction)],
                    island.rect.Width(),
                    island.rect.Height()
                );
                draw->AddText(
                    ImVec2(from.x + 6.0f, from.y + 4.0f),
                    white,
                    label.c_str()
                );
            }
            const auto caption = std::format(
                "{}  atlas {}  {:.2f} cm/texel",
                surface.Object().name,
                layout.atlasSize,
                layout.texelCm
            );
            draw->AddText(
                ImVec2(rect.x, rect.y + rect.w + 8.0f),
                white,
                caption.c_str()
            );
        }

        void onFlagsChanged(usize index) {
            const auto& f = flags[index];
            surfaces[index].SetDirections(maskOf(f), f.floorFollowsWorldUp);
            uiContext.panelDirty = true;
        }

        void exposeToPort() {
            auto* port = Port();
            if(!port)
                return;

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
            port->Expose(
                "lab",
                &settings,
                *GetDesc<PaintLabSettings>(),
                [this] { uiContext.panelDirty = true; }
            );
            for(usize i = 0; i < surfaces.size(); ++i) {
                port->Expose(
                    std::format("stage.{}", surfaces[i].Object().name),
                    &flags[i],
                    *GetDesc<PaintObjectFlags>(),
                    [this, i] { onFlagsChanged(i); }
                );
            }
        }

        Widget buildPanel() {
            auto& camera = static_cast<FlyCamera&>(Camera());
            std::vector<Widget> sections{
                buildPropertyTree(
                    "lab",
                    &settings,
                    *GetDesc<PaintLabSettings>(),
                    [] {}
                ),
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
                )
            };
            for(usize i = 0; i < surfaces.size(); ++i) {
                if(active[i] == 0)
                    continue;
                sections.push_back(buildPropertyTree(
                    std::format("stage.{}", surfaces[i].Object().name).c_str(),
                    &flags[i],
                    *GetDesc<PaintObjectFlags>(),
                    [this, i] { onFlagsChanged(i); }
                ));
            }

            return Column(std::move(sections));
        }
    };
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
