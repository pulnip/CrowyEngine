#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <imgui.h>

#include "CommandPort.hpp"
#include "DOM.hpp"
#include "FixedTickClock.hpp"
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
#include "PaintWorld.hpp"
#include "PortStatusChip.hpp"
#include "PropertyWalker.hpp"
#include "RenderApp.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    namespace
    {
        // the window is fixed; every overlay and the cursor work in it
        constexpr u32 ScreenWidth = 1920;
        constexpr u32 ScreenHeight = 1080;
    }

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
        // a splash on every grid tile at once
        static constexpr usize MaxDropletSpheres = 16 * PaintMaxDroplets;
        // flight substeps in one clock tick
        static constexpr i32 FlightSubstepsPerTick = 4;
        static_assert(
            static_cast<i64>(
                FlightSubstepsPerTick * PaintFlightSubstep * 1e9 + 0.5
            ) == TickNanoseconds
        );
        // MPC_TeamLook's base colors, Mint and Choco
        static constexpr std::array<Vec3, 2> TeamColors{
            Vec3{0.0f, 0.8f, 0.505794f},
            Vec3{0.158f, 0.04661f, 0.01738f}
        };

        struct ShapeLabView {
            f32 stretch = 1.0f;
            f32 impactU = 0.0f;
            // half the panel's width in U / R
            f32 extent = 1.2f;
        };

        RAII<PaintWorld> world = std::make_unique<PaintWorld>();
        // one per surface, in the same order: its flags as the port sees them
        std::vector<PaintObjectFlags> flags;
        std::array<GeometryAllocation, PaintMeshKindCount> geometry{};
        std::vector<PrimitiveHandle> proxies;
        LightHandle sun{};
        // a sphere per droplet in the air, hidden when there is none
        std::vector<PrimitiveHandle> dropletSpheres;
        std::array<MeshHandle, 2> dropletMeshes{};
        AABB3D sphereBounds{};
        FixedTickClock clock;
        // flight substeps run up but not yet owed under slow motion
        i32 substepCredit = 0;
        bool dumpsFrames = std::getenv("CROWY_DUMP_FRAME") != nullptr;
        RAII<PaintGpu> gpu;
        PaintLabSettings settings;
        PaintShotSettings shot;
        PaintLookSettings look;
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
                port->Unexpose("splat");
                port->Unexpose("look");
                for(const auto& surface: world->Surfaces())
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
            for(const auto& surface: world->Surfaces())
                flags.push_back(flagsOf(surface.Object()));
        }

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            for(usize kind = 0; kind < PaintMeshKindCount; ++kind) {
                const auto data = toMeshData(
                    world->Meshes().Get(static_cast<PaintMeshKind>(kind))
                );
                geometry[kind] = pool.Add(data.vertices, data.indices);
            }
        }

        void ExtractScene(RenderScene& scene) override {
            sun = scene.Lights().Add(
                LightSnapshot{
                    .castShadow = true,
                    .color = ones(),
                    .intensity = 3.0f,
                    .direction = sunDirection()
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
            for(usize i = 0; i < world->Surfaces().size(); ++i) {
                const auto& object = world->Surfaces()[i].Object();
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

            sphereBounds = localBounds(PaintMeshKind::Sphere);
            for(usize team = 0; team < dropletMeshes.size(); ++team) {
                const auto material = scene.Materials().Add(
                    MaterialResource{
                        .data =
                            {.albedo = TeamColors[team], .roughness = 0.35f},
                        .pipeline = dropletPipeline()
                    }
                );
                dropletMeshes[team] = scene.Meshes().Add(
                    MeshResource{
                        .subMeshes = {SubMesh{
                            .geometry = geometry
                                [static_cast<usize>(PaintMeshKind::Sphere)],
                            .localBounds = sphereBounds
                        }},
                        .materials = {material},
                        .localBounds = sphereBounds
                    }
                );
            }
            for(usize i = 0; i < MaxDropletSpheres; ++i) {
                dropletSpheres.push_back(scene.Primitives().Add(
                    PrimitiveSnapshot{
                        .localToWorld = unitMat(),
                        .worldBounds = sphereBounds,
                        .mesh = dropletMeshes[0],
                        .flags = PrimitiveFlags::None
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
            // UI rides this pass, so it keeps one color target and no depth
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
            // the ball leaves the eye through the cursor, as the sample
            // map's click does; Space is the camera's
            const bool clicked = input.IsKeyPressed(MouseButton::LButton) ||
                                 input.IsKeyPressed(KeyCode::F);
            if(clicked && !ImGui::GetIO().WantCaptureMouse)
                fireThroughCursor(input.GetMousePos());
        }

        void OnUpdateScene(f64 seconds) override {
            // a smoke run's picture: both teams and a splash, from frame 2,
            // when the surfaces first draw
            if(dumpsFrames && FrameNumber() == 2)
                fireSmokeVolley();

            // the droplets fly in fixed substeps: the frame's time on screen,
            // one tick a frame when a capture counts them
            const auto ticks = settings.countFrames || dumpsFrames
                                   ? 1u
                                   : clock.Advance(toNanoseconds(seconds));
            if(!settings.paused) {
                substepCredit +=
                    static_cast<i32>(ticks) * FlightSubstepsPerTick;
                const auto slow = std::max(settings.slowMotion, 1);
                const auto steps = substepCredit / slow;
                substepCredit -= steps * slow;
                world->Step(steps);
            }
            placeDropletSpheres();
            Scene().Lights().GetRef(sun).direction = sunDirection();

            if(settings.stage != shownStage) {
                shownStage = settings.stage;
                world->SetStage(stageKind());
                for(usize i = 0; i < proxies.size(); ++i)
                    Scene().Primitives().GetRef(proxies[i]).flags =
                        proxyFlags(i);
                uiContext.panelDirty = true;
            }
            world->SetShapeStage(shot.shapeStage);
        }

        std::vector<PassHook> OnRecordSimulation(
            RHICommandList& cmdList
        ) override {
            ++recordedFrames;
            gpu->Sync(world->Surfaces());
            const auto draws = world->TakeDraws();
            const auto acquires = gpu->Record(
                cmdList,
                Renderer().Pipelines(),
                world->Surfaces(),
                draws
            );

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
            drawScoreReadout();
            drawSplashDebug();

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
                .drawCapacity = 512,
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

        static MaterialPipelineDesc dropletPipeline() {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = StandardForward, .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = StandardForward, .entryPoint = "fs_opaque"},
                .rasterizer = {.frontCounterClockwise = false},
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

        // a team, a reserved id or the eraser
        static u8 clampPaintId(i64 id) {
            return static_cast<u8>(std::clamp<i64>(id, 0, PaintIdCount - 1));
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

        // a one-pixel frame just outside the panel and its caption below
        static void drawPanelFrame(
            ImDrawList& draw,
            Vec4 rect,
            CStr caption,
            f32 fontScale = 1.25f
        ) {
            const auto white = IM_COL32(255, 255, 255, 235);
            draw.AddRect(
                ImVec2(rect.x - 1.0f, rect.y - 1.0f),
                ImVec2(rect.x + rect.z + 1.0f, rect.y + rect.w + 1.0f),
                white
            );
            draw.AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize() * fontScale,
                ImVec2(rect.x, rect.y + rect.w + 8.0f),
                white,
                caption
            );
        }

        static PaintBrushProfile brushOf(PaintBrushChoice choice) {
            using enum PaintBrushChoice;

            switch(choice) {
            case Default:
                return PaintBrushProfile::Default();
            case Paintball:
                return PaintBrushProfile::Paintball();
            case MopT:
                return PaintBrushProfile::MopT();
            case Smooth:
                return PaintBrushProfile::Smooth();
            }

            return PaintBrushProfile::Default();
        }

        static std::optional<DVec3> readVec3(const DOM::Value* value) {
            const auto* array = value ? value->asArray() : nullptr;
            if(!array || array->size() != 3)
                return std::nullopt;
            DVec3 v;
            for(usize a = 0; a < 3; ++a) {
                const auto component = (*array)[a].get<f64>();
                if(!component)
                    return std::nullopt;
                v[a] = *component;
            }

            return v;
        }

        static DOM::Value toValue(DVec3 v) {
            DOM::Array array;
            array.emplace_back(v.x);
            array.emplace_back(v.y);
            array.emplace_back(v.z);

            return DOM::Value(std::move(array));
        }

        PaintStageKind stageKind() const noexcept {
            return settings.stage == PaintStageChoice::Block
                       ? PaintStageKind::Block
                       : PaintStageKind::Grid;
        }

        PrimitiveFlags proxyFlags(usize i) const {
            return world->Active()[i] != 0 ? combine(
                                                 PrimitiveFlags::Visible,
                                                 PrimitiveFlags::CastShadow
                                             )
                                           : PrimitiveFlags::None;
        }

        AABB3D localBounds(PaintMeshKind kind) const {
            const auto bounds = boundsOf(world->Meshes().Get(kind));

            return AABB3D{
                .center = toVec3(bounds.Center()),
                .halfScale = toVec3(bounds.Extent())
            };
        }

        Vec4 panelRect() const {
            const auto width = static_cast<f32>(ScreenWidth);

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
                static_cast<i32>(world->Surfaces().size()) - 1
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
                world->Surfaces(),
                world->Active(),
                geometry,
                PaintDrawSettings{
                    .view = static_cast<u32>(settings.view),
                    .compareView = static_cast<u32>(settings.compareView),
                    .showsCells = showsCells(),
                    .lookModes =
                        {lobeMask(),
                         static_cast<u32>(look.edgeMode),
                         static_cast<u32>(look.teamBlend),
                         static_cast<u32>(look.heightFilter)},
                    .lookModes2 =
                        {look.heightSource == PaintHeightSource::PaintedArea
                             ? 1u
                             : 0u,
                         look.edgeFade ? 1u : 0u,
                         look.blendableGBuffer ? 1u : 0u,
                         0u},
                    .lookStyle =
                        {look.coatScale,
                         look.fuzzScale,
                         look.roughnessBias,
                         look.flow},
                    .lookStyle2 =
                        {look.normalStrength, look.coatRoughness, 0.1f, 1.0f},
                    .splitPixels = split >= 1.0f
                                       ? 1e9f
                                       : split * static_cast<f32>(ScreenWidth)
                }
            );
        }

        u32 drawPanels(
            RHICommandList& cmdList,
            const HookPassContext& context
        ) {
            if(settings.panel == PaintPanel::None || recordedFrames < 2)
                return 0;
            if(settings.panel == PaintPanel::ShapeLab) {
                const auto lab = shapeLab();
                return gpu->DrawShapeLab(
                    cmdList,
                    context,
                    Renderer().Pipelines(),
                    PaintShapeLabPush{
                        .rect = panelRect(),
                        .stamp =
                            {static_cast<f32>(shot.seed & 0xFFFF),
                             lab.impactU,
                             lab.stretch,
                             shot.shapeStage},
                        .look = {
                            brushOf(shot.brush).shapeNoise,
                            lab.extent,
                            static_cast<f32>(clampPaintId(shot.team))
                        }
                    }
                );
            }
            const auto index = selectedSurface();

            return gpu->DrawPanel(
                cmdList,
                context,
                Renderer().Pipelines(),
                world->Surfaces()[index],
                index,
                static_cast<u32>(settings.panelChannel),
                panelRect(),
                profileLine(index).value_or(Vec4{})
            );
        }

        u32 lobeMask() const noexcept {
            return (look.diffuse ? PAINT_LOBE_DIFFUSE : 0u) |
                   (look.specular ? PAINT_LOBE_SPECULAR : 0u) |
                   (look.haze ? PAINT_LOBE_HAZE : 0u) |
                   (look.fuzz ? PAINT_LOBE_FUZZ : 0u) |
                   (look.sss ? PAINT_LOBE_SSS : 0u) |
                   (look.coat ? PAINT_LOBE_COAT : 0u) |
                   (look.sky ? PAINT_LOBE_SKY : 0u);
        }

        bool showsCells() const noexcept {
            const auto score = [](PaintView v) {
                return v == PaintView::Score || v == PaintView::Divergence;
            };
            return score(settings.view) ||
                   (settings.split < 1.0f && score(settings.compareView));
        }

        void fireSmokeVolley() {
            const auto fire =
                [&](DVec3 target, DVec3 velocity, u8 team, i32 seed) {
                    world->Fire(
                        PaintShot{
                            .origin = target - velocity * 0.3,
                            .velocity = velocity,
                            .paintId = team,
                            .seed = seed,
                            .splash = true
                        }
                    );
                };
            fire({-300.0, -600.0, 0.0}, {0.0, 0.0, -3000.0}, 0, 11);
            fire({-220.0, -560.0, 0.0}, {1200.0, 0.0, -2600.0}, 1, 12);
            fire({-150.0, -150.0, 300.0}, {0.0, 0.0, -2500.0}, 0, 13);
            fire({-420.0, -420.0, 0.0}, {1500.0, 0.0, -2500.0}, 1, 14);
        }

        // the way the sunlight travels, in the Crowy frame
        Vec3 sunDirection() const {
            constexpr auto Degree = std::numbers::pi_v<f32> / 180.0f;
            const auto azimuth = look.sunAzimuth * Degree;
            const auto elevation = look.sunElevation * Degree;

            return -Vec3{
                std::cos(elevation) * std::sin(azimuth),
                std::sin(elevation),
                std::cos(elevation) * std::cos(azimuth)
            };
        }

        void placeDropletSpheres() {
            const auto sphereRadius = sphereBounds.halfScale.x;
            usize next = 0;
            const auto place = [&](DVec3 center, f32 radius, u8 paintId) {
                if(next == dropletSpheres.size())
                    return;
                const auto scale = radius / sphereRadius;
                const auto localToWorld = mintToCrowy() *
                                          translateMat(toVec3(center)) *
                                          scaleMat(Vec3{scale, scale, scale});
                auto& sphere =
                    Scene().Primitives().GetRef(dropletSpheres[next++]);
                sphere.localToWorld = localToWorld;
                sphere.worldBounds =
                    transformAABB3D(localToWorld, sphereBounds);
                sphere.mesh = dropletMeshes[paintId & 1u];
                sphere.flags = combine(
                    PrimitiveFlags::Visible,
                    PrimitiveFlags::CastShadow
                );
            };
            for(const auto& ball: world->Balls())
                place(ball.Position(), ball.shot.ballRadius, ball.shot.paintId);
            for(const auto& flight: world->Flights()) {
                for(const auto& droplet: flight.droplets) {
                    if(droplet.alive)
                        place(droplet.position, droplet.radius, flight.paintId);
                }
            }
            for(; next < dropletSpheres.size(); ++next)
                Scene().Primitives().GetRef(dropletSpheres[next]).flags =
                    PrimitiveFlags::None;
        }

        // a Mint point to the screen, or none behind the eye
        std::optional<ImVec2> toScreen(DVec3 point) {
            const auto& camera = Camera();
            const auto p = toCrowyPoint(point);
            const auto clip = camera.Projection(Aspect()) *
                              (camera.View() * Vec4{p.x, p.y, p.z, 1.0f});
            if(clip.w <= 1e-4f)
                return std::nullopt;

            return ImVec2(
                (clip.x / clip.w * 0.5f + 0.5f) * static_cast<f32>(ScreenWidth),
                (0.5f - clip.y / clip.w * 0.5f) * static_cast<f32>(ScreenHeight)
            );
        }

        // a circle lying in the plane of `normal`, as a screen polyline
        void drawGroundCircle(
            ImDrawList& draw,
            DVec3 center,
            DVec3 normal,
            f64 radius,
            ImU32 color,
            bool filled
        ) {
            constexpr i32 Segments = 48;
            const auto reference = std::abs(normal.z) < 0.9
                                       ? DVec3{0.0, 0.0, 1.0}
                                       : DVec3{1.0, 0.0, 0.0};
            const auto u = getSafeNormal(cross(normal, reference));
            const auto v = cross(normal, u);
            std::vector<ImVec2> points;
            for(i32 i = 0; i < Segments; ++i) {
                const auto a = 2.0 * std::numbers::pi * i / Segments;
                const auto screen = toScreen(
                    center + (u * std::cos(a) + v * std::sin(a)) * radius +
                    normal * 0.5
                );
                if(!screen)
                    return;
                points.push_back(*screen);
            }
            if(filled)
                draw.AddConvexPolyFilled(points.data(), Segments, color);
            else
                draw.AddPolyline(
                    points.data(),
                    Segments,
                    color,
                    ImDrawFlags_Closed,
                    2.0f
                );
        }

        // the splash's two halves side by side: rings where the score's
        // phantoms landed, filled marks where the picture's droplets did
        void drawSplashDebug() {
            if(!settings.splashDebug)
                return;
            constexpr std::array<ImU32, 3> GroupColors{
                IM_COL32(64, 230, 170, 255),
                IM_COL32(230, 150, 90, 255),
                IM_COL32(190, 190, 190, 255)
            };
            const auto white = IM_COL32(255, 255, 255, 220);
            const auto yellow = IM_COL32(250, 210, 60, 255);
            auto* draw = ImGui::GetForegroundDrawList();
            const auto& splash = world->Splash();

            for(const auto& mark: world->Marks())
                drawGroundCircle(
                    *draw,
                    mark.point,
                    mark.normal,
                    mark.radius,
                    IM_COL32(255, 255, 255, 70),
                    true
                );
            for(const auto& phantom: world->Phantoms())
                drawGroundCircle(
                    *draw,
                    phantom.point,
                    phantom.normal,
                    phantom.radius,
                    yellow,
                    false
                );
            for(const auto& flight: world->Flights()) {
                drawGroundCircle(
                    *draw,
                    flight.contact,
                    flight.normal,
                    flight.markClearance,
                    white,
                    false
                );
                drawGroundCircle(
                    *draw,
                    flight.contact,
                    flight.normal,
                    splash.maxTravel,
                    IM_COL32(255, 255, 255, 90),
                    false
                );
                for(const auto& droplet: flight.droplets) {
                    if(!droplet.alive)
                        continue;
                    const auto from = toScreen(droplet.position);
                    const auto to =
                        toScreen(droplet.position + droplet.velocity * 0.05);
                    if(!from || !to)
                        continue;
                    const auto color =
                        GroupColors[static_cast<usize>(droplet.group)];
                    draw->AddLine(*from, *to, color, 2.0f);
                    draw->AddCircle(
                        *from,
                        droplet.mayMark ? 5.0f : 3.0f,
                        color
                    );
                }
            }
            if(settings.labels) {
                draw->AddText(
                    ImGui::GetFont(),
                    ImGui::GetFontSize() * 1.25f,
                    ImVec2(PanelMargin, PanelMargin),
                    white,
                    "yellow ring: a phantom landing the score took\n"
                    "white fill: a droplet's mark, the picture only\n"
                    "forward / side / back"
                );
            }
        }

        // what the CPU grid says each team owns: the only number a match is
        // decided by
        void drawScoreReadout() {
            if(!settings.labels || !showsCells())
                return;
            const auto coverage = world->Coverage();
            constexpr std::array<CStr, 2> Teams{"Mint", "Choco"};
            auto text = std::format(
                "score from the cell grid, {:.2f} m2 paintable",
                coverage.totalArea / 10000.0f
            );
            for(u8 team = 0; team < 2; ++team) {
                text += std::format(
                    "\n{}  {:.2f}%",
                    Teams[team],
                    100.0f * coverage.Fraction(team)
                );
            }
            auto* draw = ImGui::GetForegroundDrawList();
            draw->AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize() * 1.4f,
                ImVec2(PanelMargin, static_cast<f32>(ScreenHeight) - 130.0f),
                IM_COL32(255, 255, 255, 235),
                text.c_str()
            );
        }

        // the line the height profile reads: through the last splat that
        // reached the surface, along its AxisU, in atlas texels
        std::optional<Vec4> profileLine(usize index) const {
            const auto& surface = world->Surfaces()[index];
            const auto& layout = surface.Layout();
            for(auto it = world->Log().rbegin(); it != world->Log().rend();
                ++it) {
                if(it->transient)
                    continue;
                std::vector<usize> reached;
                PaintSceneQuery::OverlapSphere(
                    it->location,
                    it->WorldExtent(),
                    world->Surfaces(),
                    world->Active(),
                    reached
                );
                if(std::ranges::find(reached, index) == reached.end())
                    continue;

                const auto stamp = surface.ComputeLocalStamp(*it);
                const auto* island =
                    layout.Find(classifyPaintFaceDirection(stamp.normal));
                if(!island)
                    return std::nullopt;
                const auto bounds = surface.ScaledBounds();
                const auto reach = 1.4 * stamp.radius * stamp.stretch;
                const auto toTexel = [&](DVec3 local) {
                    return island->ProjectNormalized(
                        (local - bounds.min) / bounds.Size()
                    );
                };
                const auto from = toTexel(stamp.center - stamp.axisU * reach);
                const auto to = toTexel(stamp.center + stamp.axisU * reach);

                return Vec4{
                    static_cast<f32>(from.x),
                    static_cast<f32>(from.y),
                    static_cast<f32>(to.x),
                    static_cast<f32>(to.y)
                };
            }

            return std::nullopt;
        }

        // BuildSplat's stretch and ImpactU for the shot settings' incidence
        ShapeLabView shapeLab() const {
            // a unit velocity: at zero speed BuildSplat falls back to head-on
            const auto theta = std::clamp(shot.theta, 0.0f, 89.0f) *
                               std::numbers::pi_v<f32> / 180.0f;
            const auto splat =
                brushOf(shot.brush)
                    .BuildSplat(
                        PaintHit{},
                        DVec3{std::sin(theta), 0.0, -std::cos(theta)},
                        0,
                        1.0f,
                        0.0f,
                        shot.seed
                    );

            return ShapeLabView{
                .stretch = splat.stretch,
                .impactU = splat.impactU,
                .extent = 1.15f * splat.stretch
            };
        }

        void drawShapeLabLabels() {
            constexpr std::array<CStr, 9> Stages{
                "0  circle: |p| - 0.5",
                "1  ellipse: stretch, cut by the ellipsoid",
                "2  wobble: 2nd, 3rd, 5th harmonics",
                "3  even petals, hard union: the sunflower",
                "4  seeded satellites",
                "5  spike field and tear",
                "6  smooth-min union",
                "7  edge crinkle",
                "8  MintChoco's stamp"
            };
            const auto lab = shapeLab();
            const auto rect = panelRect();
            auto* draw = ImGui::GetForegroundDrawList();
            const auto white = IM_COL32(255, 255, 255, 235);
            const auto stage = std::clamp(shot.shapeStage, 0.0f, 8.0f);
            const auto text = std::format(
                "{}\nstage {:.2f}  seed {}  theta {:.0f} deg  S {:.2f}  "
                "ImpactU {:.3f}",
                Stages[static_cast<usize>(std::floor(stage))],
                stage,
                shot.seed & 0xFFFF,
                shot.theta,
                lab.stretch,
                lab.impactU
            );
            drawPanelFrame(*draw, rect, text.c_str());
            // the contact, behind the centre on a grazing hit, and the travel
            const auto toPanel = [&](f32 u, f32 v) {
                return ImVec2(
                    rect.x + (u / lab.extent * 0.5f + 0.5f) * rect.z,
                    rect.y + (0.5f - v / lab.extent * 0.5f) * rect.w
                );
            };
            const auto impact = toPanel(lab.impactU * lab.stretch, 0.0f);
            draw->AddCircleFilled(impact, 6.0f, IM_COL32(245, 184, 51, 255));
            draw->AddLine(
                toPanel(-0.95f * lab.extent, -0.9f * lab.extent),
                toPanel(-0.55f * lab.extent, -0.9f * lab.extent),
                white,
                2.0f
            );
            draw->AddText(
                toPanel(-0.95f * lab.extent, -0.82f * lab.extent),
                white,
                "AxisU"
            );
        }

        // the panel's island rectangles and names, and what the layout is
        void drawPanelLabels() {
            if(settings.panel == PaintPanel::ShapeLab && settings.labels) {
                drawShapeLabLabels();
                return;
            }
            if(settings.panel == PaintPanel::None || !settings.labels)
                return;
            if(settings.panelChannel == PaintPanelChannel::Profile) {
                drawPanelFrame(
                    *ImGui::GetForegroundDrawList(),
                    panelRect(),
                    "G across the last splat, along AxisU\n"
                    "a line every 0.35, the top line 1.0 = 9 cm"
                );
                return;
            }
            const auto& surface = world->Surfaces()[selectedSurface()];
            const auto& layout = surface.Layout();
            if(layout.atlasSize == 0)
                return;

            auto* draw = ImGui::GetForegroundDrawList();
            const auto rect = panelRect();
            const auto scale = rect.z / static_cast<f32>(layout.atlasSize);
            const auto white = IM_COL32(255, 255, 255, 230);
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
            drawPanelFrame(*draw, rect, caption.c_str(), 1.0f);
        }

        void onFlagsChanged(usize index) {
            const auto& f = flags[index];
            world->SetDirections(index, maskOf(f), f.floorFollowsWorldUp);
            uiContext.panelDirty = true;
        }

        // the shot settings as a shot from `origin` along `direction`
        PaintShot shotFrom(DVec3 origin, DVec3 direction) const {
            return PaintShot{
                .origin = origin,
                .velocity = getSafeNormal(direction) * shot.speed,
                .paintId = clampPaintId(shot.team),
                .seed = shot.seed,
                .volume = shot.volume,
                .heightAdd = shot.heightAdd,
                .brush = brushOf(shot.brush),
                .ballRadius = shot.ballRadius,
                .splash = shot.splash
            };
        }

        void fireThroughCursor(Vec2 mouse) {
            const auto& camera = static_cast<FlyCamera&>(Camera());
            const auto projection = camera.Projection(Aspect());
            const auto ndcX =
                2.0f * mouse.x / static_cast<f32>(ScreenWidth) - 1.0f;
            const auto ndcY =
                1.0f - 2.0f * mouse.y / static_cast<f32>(ScreenHeight);
            const Vec3 viewDirection{
                ndcX / projection[0].x,
                ndcY / projection[1].y,
                1.0f
            };
            const auto rotation = rotateMat(camera.Rotation());
            const auto direction = static_cast<Vec3>(
                rotation *
                Vec4{viewDirection.x, viewDirection.y, viewDirection.z, 0.0f}
            );
            world->Fire(shotFrom(
                fromCrowyPoint(camera.Position()),
                fromCrowyVector(direction)
            ));
            if(shot.autoSeed)
                shot.seed = (shot.seed + 1) & 0xFFFF;
            uiContext.panelDirty = true;
        }

        void registerVerbs(CommandPort& port) {
            // a shot in Mint cm: origin, then a target or a direction
            port.RegisterVerb(
                "paint_fire",
                [this](const DOM::Value& args, Reply reply) {
                    const auto origin = readVec3(args.at("origin"));
                    const auto target = readVec3(args.at("target"));
                    const auto direction = readVec3(args.at("direction"));
                    if(!origin || (!target && !direction)) {
                        reply.Error(
                            "paint_fire takes origin and a target or a "
                            "direction, each [x, y, z] in cm"
                        );
                        return;
                    }

                    auto request = shotFrom(
                        *origin,
                        target ? *target - *origin : *direction
                    );
                    const auto speed =
                        args.get<f64>("speed").value_or(shot.speed);
                    request.velocity = getSafeNormal(request.velocity) * speed;
                    request.paintId =
                        clampPaintId(args.get<i64>("team").value_or(shot.team));
                    request.seed = static_cast<i32>(
                        args.get<i64>("seed").value_or(shot.seed)
                    );
                    request.volume = static_cast<f32>(
                        args.get<f64>("volume").value_or(shot.volume)
                    );
                    request.heightAdd = static_cast<f32>(
                        args.get<f64>("heightAdd").value_or(shot.heightAdd)
                    );
                    request.splash =
                        args.get<bool>("splash").value_or(shot.splash);
                    request.ballRadius = static_cast<f32>(
                        args.get<f64>("ballRadius").value_or(shot.ballRadius)
                    );
                    request.onlySurface =
                        args.get<i64>("onlySurface").value_or(-1);
                    if(const auto brush = args.get<Str>("brush")) {
                        const auto choice =
                            enumFromName<PaintBrushChoice>(*brush);
                        if(!choice) {
                            reply.Error(
                                std::format("unknown brush {}", *brush)
                            );
                            return;
                        }
                        request.brush = brushOf(*choice);
                    }

                    // seconds of flight shown before the contact
                    if(const auto lead = args.get<f64>("lead")) {
                        DOM::Table result;
                        result.emplace(
                            "launched",
                            DOM::Value(world->Launch(request, *lead))
                        );
                        reply.Ok(DOM::Value(std::move(result)));
                        return;
                    }
                    const auto before = world->Log().size();
                    const auto hit = world->Fire(request);
                    DOM::Table result;
                    result.emplace("hit", DOM::Value(hit.has_value()));
                    if(hit && world->Log().size() > before) {
                        const auto& splat = world->Log().back();
                        const auto& name =
                            world->Surfaces()[static_cast<usize>(hit->surface)]
                                .Object()
                                .name;
                        result.emplace("surface", DOM::Value(name));
                        result.emplace(
                            "transient",
                            DOM::Value(splat.transient)
                        );
                        result.emplace("location", toValue(splat.location));
                        result.emplace(
                            "radius",
                            DOM::Value(static_cast<f64>(splat.radius))
                        );
                        result.emplace(
                            "stretch",
                            DOM::Value(static_cast<f64>(splat.stretch))
                        );
                        result.emplace(
                            "impactU",
                            DOM::Value(static_cast<f64>(splat.impactU))
                        );
                        result.emplace(
                            "seed",
                            DOM::Value(static_cast<i64>(splat.seed))
                        );
                        result.emplace(
                            "phantoms",
                            DOM::Value(
                                static_cast<i64>(world->Phantoms().size())
                            )
                        );
                    }
                    reply.Ok(DOM::Value(std::move(result)));
                }
            );
            port.RegisterVerb(
                "paint_reset",
                [this](const DOM::Value&, Reply reply) {
                    world->Reset();
                    gpu->ClearAll();
                    reply.Ok();
                }
            );
            // the splashes in the air and what they left: droplets, the
            // score's phantom landings and the picture's marks
            port.RegisterVerb(
                "paint_droplets",
                [this](const DOM::Value&, Reply reply) {
                    const auto marks =
                        [](std::span<const PaintSplashMark> all) {
                            DOM::Array array;
                            for(const auto& mark: all) {
                                DOM::Table entry;
                                entry.emplace("point", toValue(mark.point));
                                entry.emplace(
                                    "radius",
                                    DOM::Value(static_cast<f64>(mark.radius))
                                );
                                array.emplace_back(std::move(entry));
                            }
                            return DOM::Value(std::move(array));
                        };
                    DOM::Array flights;
                    for(const auto& flight: world->Flights()) {
                        DOM::Array droplets;
                        for(const auto& droplet: flight.droplets) {
                            DOM::Table entry;
                            entry.emplace(
                                "position",
                                toValue(droplet.position)
                            );
                            entry.emplace(
                                "velocity",
                                toValue(droplet.velocity)
                            );
                            entry.emplace(
                                "radius",
                                DOM::Value(static_cast<f64>(droplet.radius))
                            );
                            entry.emplace(
                                "group",
                                DOM::Value(static_cast<i64>(droplet.group))
                            );
                            entry.emplace("alive", DOM::Value(droplet.alive));
                            entry.emplace(
                                "mayMark",
                                DOM::Value(droplet.mayMark)
                            );
                            droplets.emplace_back(std::move(entry));
                        }
                        DOM::Table entry;
                        entry.emplace("contact", toValue(flight.contact));
                        entry.emplace(
                            "markClearance",
                            DOM::Value(static_cast<f64>(flight.markClearance))
                        );
                        entry.emplace(
                            "droplets",
                            DOM::Value(std::move(droplets))
                        );
                        flights.emplace_back(std::move(entry));
                    }
                    DOM::Table result;
                    result.emplace("flights", DOM::Value(std::move(flights)));
                    result.emplace("phantoms", marks(world->Phantoms()));
                    result.emplace("marks", marks(world->Marks()));
                    reply.Ok(DOM::Value(std::move(result)));
                }
            );
            // a surface's atlas, as MintChoco logs it
            port.RegisterVerb(
                "paint_layout",
                [this](const DOM::Value& args, Reply reply) {
                    const auto index =
                        args.get<i64>("surface").value_or(settings.selected);
                    const auto count =
                        static_cast<i64>(world->Surfaces().size());
                    if(index < 0 || index >= count) {
                        reply.Error(
                            "surface is an index into the stage's objects"
                        );
                        return;
                    }
                    const auto& surface =
                        world->Surfaces()[static_cast<usize>(index)];
                    const auto& layout = surface.Layout();
                    DOM::Table result;
                    result.emplace("name", DOM::Value(surface.Object().name));
                    result.emplace("atlas", DOM::Value(layout.ToString()));
                    result.emplace(
                        "texelCm",
                        DOM::Value(static_cast<f64>(layout.texelCm))
                    );
                    result.emplace(
                        "atlasSize",
                        DOM::Value(static_cast<i64>(layout.atlasSize))
                    );
                    reply.Ok(DOM::Value(std::move(result)));
                }
            );
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
            port->Expose("splat", &shot, *GetDesc<PaintShotSettings>(), [this] {
                uiContext.panelDirty = true;
            });
            port->Expose("look", &look, *GetDesc<PaintLookSettings>(), [this] {
                uiContext.panelDirty = true;
            });
            registerVerbs(*port);
            for(usize i = 0; i < world->Surfaces().size(); ++i) {
                port->Expose(
                    std::format("stage.{}", world->Surfaces()[i].Object().name),
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
                    "splat",
                    &shot,
                    *GetDesc<PaintShotSettings>(),
                    [] {}
                ),
                buildPropertyTree(
                    "look",
                    &look,
                    *GetDesc<PaintLookSettings>(),
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
            for(usize i = 0; i < world->Surfaces().size(); ++i) {
                if(world->Active()[i] == 0)
                    continue;
                sections.push_back(buildPropertyTree(
                    std::format("stage.{}", world->Surfaces()[i].Object().name)
                        .c_str(),
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
        .width = ScreenWidth,
        .height = ScreenHeight,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<PaintLab>(argc, argv, windowConfig);
}
