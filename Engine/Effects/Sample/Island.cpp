#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "EffectSystem.hpp"
#include "EnumUtil.hpp"
#include "FlyCamera.hpp"
#include "Geometry/Overlap3D.hpp"
#include "Island/IslandEffects.hpp"
#include "Island/IslandScene.h"
#include "Island/IslandShapes.hpp"
#include "Island/Weather.hpp"
#include "IslandMeshes.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "ParticleEffects.hpp"
#include "PipelineCache.hpp"
#include "RenderApp.hpp"
#include "StandardPipeline.hpp"

namespace Crowy
{
    // A night island whose effects draw in a hook pass before the glass, one
    // step a recorded frame, so frame 60 is the same picture in every run.
    class Island final: public RenderApp {
        // the sky's draw: the camera's basis scaled to the frame, the moon
        struct SkyPush {
            Vec4 right;
            Vec4 up;
            Vec4 forward;
            Vec4 toMoon;
        };

        static constexpr CStr EffectsHook = "effects";
        static constexpr Vec3 FirePosition{ISLAND_FIRE};
        static constexpr f32 FireIntensity = 5.0f;
        static constexpr Vec3 ToMoon{ISLAND_TO_MOON};
        // the canvas's glow from the fire behind it, before the breath
        static constexpr Vec3 CanvasGlow{0.075f, 0.032f, 0.008f};
        static constexpr f32 PoleRadius = 0.035f;
        // how far the poles reach past their crossing
        static constexpr f32 PoleOverhang = 1.0f;
        static constexpr f32 LogRadius = 0.045f;
        // the tripod's logs stand on this circle and cross at this height
        static constexpr f32 LogCircle = 0.36f;
        static constexpr f32 LogCrossing = 1.15f;
        static constexpr f32 LogOverhang = 0.12f;

        GeometryAllocation sea{};
        GeometryAllocation island{};
        GeometryAllocation box{};
        GeometryAllocation canvas{};
        GeometryAllocation pole{};
        GeometryAllocation log{};
        LightHandle fire{};
        MaterialHandle canvasMaterial{};
        std::unique_ptr<EffectSystem> effects;

    public:
        Island()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            const auto seaMesh = MakePlane(Vec2{30.0f, 30.0f});
            const auto islandMesh = makeEllipsoid(IslandRadii);
            const auto boxMesh = MakeBox(1.0f);
            const auto canvasMesh = makeTipiCanvas();
            const auto poleMesh =
                makeCylinder(PoleRadius, 0.5f * poleLength(), 8);
            const auto logMesh = makeCylinder(LogRadius, 0.5f * logLength(), 8);

            sea = pool.Add(seaMesh.vertices, seaMesh.indices);
            island = pool.Add(islandMesh.vertices, islandMesh.indices);
            box = pool.Add(boxMesh.vertices, boxMesh.indices);
            canvas = pool.Add(canvasMesh.vertices, canvasMesh.indices);
            pole = pool.Add(poleMesh.vertices, poleMesh.indices);
            log = pool.Add(logMesh.vertices, logMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            scene.Lights().Add(
                LightSnapshot{
                    .castShadow = true,
                    .color = {ISLAND_MOON_COLOR},
                    .intensity = ISLAND_MOON_INTENSITY,
                    .direction = -ToMoon
                }
            );
            fire = scene.Lights().Add(
                LightSnapshot{
                    .kind = LightKind::Point,
                    .color = {1.0f, 0.55f, 0.2f},
                    .intensity = FireIntensity,
                    .position = {ISLAND_FIRE_LIGHT},
                    .range = 4.0f
                }
            );
            scene.Environment() = EnvironmentSnapshot{
                .skyAmbient = {0.03f, 0.04f, 0.08f},
                .groundAmbient = {0.01f, 0.01f, 0.012f}
            };

            const auto water = addMaterial(
                scene,
                {.albedo = {0.02f, 0.05f, 0.1f}, .roughness = 0.25f}
            );
            const auto sand = addMaterial(
                scene,
                {.albedo = {0.55f, 0.48f, 0.36f}, .roughness = 0.95f}
            );
            const auto wood = addMaterial(
                scene,
                {.albedo = {0.32f, 0.22f, 0.14f}, .roughness = 0.85f}
            );
            const auto charcoal = addMaterial(
                scene,
                {.albedo = {0.08f, 0.06f, 0.05f}, .roughness = 0.9f}
            );
            const auto coals = addMaterial(
                scene,
                {
                    .albedo = {0.1f, 0.05f, 0.02f},
                    .emissive = {6.0f, 2.2f, 0.5f},
                    .roughness = 0.9f
                }
            );
            // both sides: moonlit outside, firelit inside
            canvasMaterial = scene.Materials().Add(
                MaterialResource{
                    .data =
                        {.albedo = {0.62f, 0.52f, 0.38f},
                         .emissive = CanvasGlow,
                         .roughness = 0.9f},
                    .pipeline = opaquePipeline(RHICullMode::None)
                }
            );

            // casting nothing, the sea stays out of the shadow's fit
            add(
                scene,
                water,
                sea,
                unitMat(),
                {30.0f, 0.001f, 30.0f},
                PrimitiveFlags::Visible
            );
            // an ellipsoid whose crown is the fire's ground
            add(
                scene,
                sand,
                island,
                translateMat({0.0f, ISLAND_CENTER_Y, 0.0f}),
                IslandRadii
            );
            addTipi(scene, wood);
            addFire(scene, charcoal, coals);

            // the ripples blend over what is drawn before them, so they lead
            effects = std::make_unique<EffectSystem>(Device());
            effects->Add(rainDesc());
            effects->Add(starsDesc());
            effects->Add(meteorsDesc());
            effects->Add(flamesDesc());
            effects->Add(embersDesc());
        }

        FramePipelineDesc DescribePipeline(
            const StandardPipelineConfig& config
        ) override {
            auto desc = makeStandardPipeline(config);
            // a data view is the standard list: it shows the scene's data
            // alone, and FindHook then records no simulation
            if(config.post != std::vector<PostPassDesc>{tonemapPass()})
                return desc;

            const auto translucent =
                std::ranges::find(desc.passes, "Translucent", &PassDesc::name);
            const auto depth = std::ranges::find(
                desc.targets,
                "SceneDepth",
                &FrameTargetDesc::name
            );
            if(translucent == desc.passes.end() || depth == desc.targets.end())
                return desc;

            // the glass blends over the effects, which test against the
            // opaque scene's depth and keep it
            const auto sceneDepth =
                static_cast<FrameTargetID>(depth - desc.targets.begin() + 1);
            desc.passes.insert(
                translucent,
                PassDesc{
                    .name = "Effects",
                    .colors = {ColorTargetUse{
                        .target = desc.sceneColor,
                        .load = RHILoadAction::Load
                    }},
                    .depth = DepthTargetUse{
                        .target = sceneDepth,
                        .load = RHILoadAction::Load
                    },
                    .kind = HookPassDesc{.hook = EffectsHook}
                }
            );

            return desc;
        }

        // the fire breathes on the world's loop, and its glow through the
        // canvas with it
        void OnUpdateScene(f64) override {
            const auto breath = fireBreath(worldStep());
            Scene().Lights().GetRef(fire).intensity = FireIntensity * breath;
            Scene().Materials().GetRef(canvasMaterial).data.emissive =
                CanvasGlow * breath;
        }

        // the effects' steps too, so spawn and update reload with the draws
        usize OnReloadShaders() override { return effects->ReloadKernels(); }

        std::vector<PassHook> OnRecordSimulation(
            RHICommandList& cmdList
        ) override {
            std::vector<PassHook> hooks;
            if(!FindHook(EffectsHook))
                return hooks;

            const auto releases = effects->Simulate(
                cmdList,
                effectViewOf(Camera().View()),
                worldStep()
            );
            hooks.push_back(PassHook{
                .name = EffectsHook,
                .bufferAcquires = releases,
                .record =
                    [this](
                        RHICommandList& cmdList,
                        const HookPassContext& context
                    ) {
                        drawSky(cmdList, context);
                        return 1 + effects->Draw(
                                       cmdList,
                                       context,
                                       Renderer().Pipelines()
                                   );
                    }
            });

            return hooks;
        }

    private:
        static Config makeConfig() {
            return Config{
                .clearColor = {ISLAND_FAR_SEA, 1.0f},
                .drawCapacity = 32,
                .materialCapacity = 16,
                .shadowMapSize = 2048,
                .vertexPoolCapacity = 4096,
                .indexPoolCapacity = 16384
            };
        }

        // over the shallows, looking a little up: the tipi's opening ahead,
        // the moon and its path on the water to the right
        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.3f, 1.8f, -9.5f},
                .pitch = -0.04f,
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 320.0f
            };
        }

        static MaterialPipelineDesc opaquePipeline(
            RHICullMode cull = RHICullMode::Back
        ) {
            constexpr CStr StandardForward =
                "Engine/Render/Shader/StandardForward.slang";

            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = StandardForward, .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = StandardForward, .entryPoint = "fs_opaque"},
                .rasterizer =
                    {.cullMode = cull, .frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        static MaterialHandle addMaterial(
            RenderScene& scene,
            MaterialData data
        ) {
            return scene.Materials().Add(
                MaterialResource{.data = data, .pipeline = opaquePipeline()}
            );
        }

        // the tipi's poles: from the base, through the crossing, and on
        static f32 poleLength() {
            const auto rise = ISLAND_TIPI_APEX_Y - ISLAND_TIPI_BASE_Y;

            return std::hypot(ISLAND_TIPI_RADIUS, rise) + PoleOverhang;
        }

        static f32 logLength() {
            const auto rise = LogCrossing - FirePosition.y;

            return std::hypot(LogCircle, rise) + LogOverhang;
        }

        // the rotation that turns +y onto `direction`
        static Vec4 turnUpTo(Vec3 direction) {
            const auto axis = normalize(cross(unitY(), direction));

            return axisAngle(axis, std::acos(dot(unitY(), direction)));
        }

        // a triangle over the screen at the far plane, tested against the
        // scene's depth and writing none
        static RHIGraphicsPipelineStateDesc skyPipelineDesc(
            const HookPassFormats& formats
        ) {
            constexpr CStr SkyShader = "Engine/Effects/Sample/Island/Sky.slang";

            RHIGraphicsPipelineStateDesc desc{
                .preRasterizer =
                    RHILegacyFrontendDesc{
                        .topology = RHIPrimitiveTopology::TriangleList,
                        .vertexShader =
                            RHIShaderDesc{
                                .path = SkyShader,
                                .entryPoint = "vs_sky"
                            }
                    },
                .rasterizer =
                    RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader =
                    RHIShaderDesc{.path = SkyShader, .entryPoint = "fs_sky"},
                .renderTargetCount = formats.colors.size(),
                .profile = "sm_6_8"
            };
            std::ranges::copy(formats.colors, desc.renderTargetFormats.begin());
            desc.depthStencil = RHIDepthStencilState{
                .format = formats.depth,
                .depthWriteEnable = false,
                .depthFunc = RHIComparisonFunc::LessEqual
            };

            return desc;
        }

        // a mesh placed by `localToWorld`; `localHalf` is its half extent
        // around its origin
        static void add(
            RenderScene& scene,
            MaterialHandle material,
            const GeometryAllocation& geometry,
            const Mat4& localToWorld,
            Vec3 localHalf,
            PrimitiveFlags flags =
                combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
        ) {
            const AABB3D local{.center = zeros(), .halfScale = localHalf};
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes =
                        {SubMesh{.geometry = geometry, .localBounds = local}},
                    .materials = {material},
                    .localBounds = local
                }
            );
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = localToWorld,
                    .worldBounds = transformAABB3D(localToWorld, local),
                    .mesh = mesh,
                    .flags = flags
                }
            );
        }

        // a cylinder mesh from `from` along `direction`, `length` long
        static void addRod(
            RenderScene& scene,
            MaterialHandle material,
            const GeometryAllocation& geometry,
            Vec3 from,
            Vec3 direction,
            f32 radius,
            f32 length
        ) {
            const auto center = from + direction * (0.5f * length);
            add(
                scene,
                material,
                geometry,
                modelMat(center, turnUpTo(direction), ones()),
                {radius, 0.5f * length, radius}
            );
        }

        // the unit box at `position`, stretched along its faces by `scale`
        void addBox(
            RenderScene& scene,
            MaterialHandle material,
            Vec3 position,
            Vec3 scale
        ) const {
            add(
                scene,
                material,
                box,
                translateMat(position) * scaleMat(scale),
                ones()
            );
        }

        // the canvas over all but the open facets, and the poles crossing
        // above it
        void addTipi(RenderScene& scene, MaterialHandle wood) const {
            constexpr auto Middle =
                0.5f * (ISLAND_TIPI_BASE_Y + ISLAND_TIPI_CANVAS_TOP_Y);
            constexpr auto HalfHeight =
                0.5f * (ISLAND_TIPI_CANVAS_TOP_Y - ISLAND_TIPI_BASE_Y);
            constexpr Vec3 Apex{0.0f, ISLAND_TIPI_APEX_Y, 0.0f};

            add(
                scene,
                canvasMaterial,
                canvas,
                translateMat({0.0f, Middle, 0.0f}),
                {ISLAND_TIPI_RADIUS, HalfHeight, ISLAND_TIPI_RADIUS}
            );
            for(u32 k = 0; k < ISLAND_TIPI_FACETS; ++k) {
                const auto azimuth = ISLAND_TIPI_FIRST_POLE +
                                     TipiFacetAngle * static_cast<f32>(k);
                const Vec3 base{
                    ISLAND_TIPI_RADIUS * std::sin(azimuth),
                    ISLAND_TIPI_BASE_Y,
                    ISLAND_TIPI_RADIUS * std::cos(azimuth)
                };
                addRod(
                    scene,
                    wood,
                    pole,
                    base,
                    normalize(Apex - base),
                    PoleRadius,
                    poleLength()
                );
            }
        }

        // the glowing bed and three logs leaning into a tripod over it
        void addFire(
            RenderScene& scene,
            MaterialHandle charcoal,
            MaterialHandle coals
        ) const {
            constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;
            constexpr Vec3 Crossing{0.0f, LogCrossing, 0.0f};

            addBox(
                scene,
                coals,
                FirePosition + Vec3{0.0f, 0.03f, 0.0f},
                {0.16f, 0.03f, 0.16f}
            );
            for(u32 i = 0; i < 3; ++i) {
                const auto azimuth = TwoPi * static_cast<f32>(i) / 3.0f;
                const Vec3 offset{
                    LogCircle * std::sin(azimuth),
                    0.03f,
                    LogCircle * std::cos(azimuth)
                };
                const auto base = FirePosition + offset;
                addRod(
                    scene,
                    charcoal,
                    log,
                    base,
                    normalize(Crossing - base),
                    LogRadius,
                    logLength()
                );
            }
        }

        // the night sky first, where the scene left the depth clear
        void drawSky(RHICommandList& cmdList, const HookPassContext& context) {
            const auto view = Camera().View();
            const auto projection = Camera().Projection(Aspect());
            const auto basis = effectViewOf(view);
            const SkyPush push{
                .right = toVec4(basis.right / projection[0].x, 0.0f),
                .up = toVec4(basis.up / projection[1].y, 0.0f),
                .forward = toVec4(Vec3{view[0].z, view[1].z, view[2].z}, 0.0f),
                .toMoon = toVec4(ToMoon, 0.0f)
            };
            cmdList.SetPipelineState(
                Renderer().Pipelines().Resolve(skyPipelineDesc(context.formats))
            );
            cmdList.SetPushGraphicsConstants(push);
            cmdList.Draw(3, 1);
        }

        // one step a frame, the clock the effects and the fire's light share
        u32 worldStep() const { return static_cast<u32>(FrameNumber()); }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "Island",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<Island>(argc, argv, windowConfig);
}
