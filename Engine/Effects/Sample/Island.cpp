#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "CameraComponent.hpp"
#include "CommandPort.hpp"
#include "EffectComponent.hpp"
#include "EffectSystem.hpp"
#include "EnumUtil.hpp"
#include "Expected.hpp"
#include "FlyCamera.hpp"
#include "FlyControl.hpp"
#include "Geometry/Overlap3D.hpp"
#include "Island/IslandEffects.hpp"
#include "Island/IslandScene.h"
#include "Island/IslandShapes.hpp"
#include "Island/Weather.hpp"
#include "IslandMeshes.hpp"
#include "LightComponent.hpp"
#include "LinearAlgebra.hpp"
#include "MeshComponent.hpp"
#include "MeshGenerator.hpp"
#include "ParticleEffects.hpp"
#include "PipelineCache.hpp"
#include "RenderApp.hpp"
#include "ScriptComponent.hpp"
#include "SnapshotCamera.hpp"
#include "StandardFrame.hpp"
#include "World.hpp"
#include "WorldClock.hpp"

namespace Crowy
{
    // A night island whose effects draw in a hook pass before the glass,
    // stepped by the World's fixed ticks.
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
        static constexpr f32 LogRadius = 0.045f;
        // drawn and casting, as every mesh but the sea
        static constexpr auto Solid =
            combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow);

        GeometryAllocation sea{};
        GeometryAllocation island{};
        GeometryAllocation box{};
        GeometryAllocation canvas{};
        GeometryAllocation pole{};
        GeometryAllocation log{};
        MaterialHandle canvasMaterial{};
        RAII<EffectSystem> effects;
        World world;
        // what the fire's bed, logs, light and effects ride
        ActorId fire;
        ActorId cameraActor;
        FlyState fly;
        // last, so the port lets go of the members above before they go
        PortExposures exposures;

    public:
        ~Island() override = default;
        CROWY_DECLARE_PINNED(Island)

        Island()
            : RenderApp(makeConfig(), std::make_unique<SnapshotCamera>()) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            // from under the sand out to where it has faded into the far sea
            const auto seaMesh = makeSeaGrid(3.5f, 280.0f, 256);
            const auto islandMesh = makeEllipsoid(IslandRadii);
            const auto boxMesh = MakeBox(1.0f);
            const auto canvasMesh = makeTipiCanvas();
            const auto poleMesh =
                makeCylinder(PoleRadius, 0.5f * poleLength(), 8);
            const auto logMesh = makeCylinder(LogRadius, 0.5f * logLength(), 8);

            sea = valueOrThrow(pool.Add(seaMesh.vertices, seaMesh.indices));
            island =
                valueOrThrow(pool.Add(islandMesh.vertices, islandMesh.indices));
            box = valueOrThrow(pool.Add(boxMesh.vertices, boxMesh.indices));
            canvas =
                valueOrThrow(pool.Add(canvasMesh.vertices, canvasMesh.indices));
            pole = valueOrThrow(pool.Add(poleMesh.vertices, poleMesh.indices));
            log = valueOrThrow(pool.Add(logMesh.vertices, logMesh.indices));
        }

        void ExtractScene(RenderScene& scene) override {
            world.Provide(scene);
            world.Provide(static_cast<SnapshotCamera&>(Camera()).Snapshot());

            const auto start = makeCamera();
            fly = FlyState(CameraLook{.yaw = start.yaw, .pitch = start.pitch});
            cameraActor = world.Spawn(
                Transform{
                    .position = start.position,
                    .rotation = flyRotation(fly.look.yaw, fly.look.pitch)
                }
            );
            world.Add(
                cameraActor,
                CameraComponent{
                    .fovY = start.fovY,
                    .nearZ = start.nearZ,
                    .farZ = start.farZ
                }
            );

            // the moon's light travels along its actor's +Z, away from it;
            // it lands first, so it is the shadow's caster
            const auto moon =
                world.Spawn(Transform{.rotation = turnTo(unitZ(), -ToMoon)});
            world.Add(
                moon,
                LightComponent{
                    .castShadow = true,
                    .color = {ISLAND_MOON_COLOR},
                    .intensity = ISLAND_MOON_INTENSITY
                }
            );
            scene.Environment() = EnvironmentSnapshot{
                .skyAmbient = {0.03f, 0.04f, 0.08f},
                .groundAmbient = {0.01f, 0.01f, 0.012f}
            };

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
                {.albedo = {0.1f, 0.05f, 0.02f},
                 .emissive = {6.0f, 2.2f, 0.5f},
                 .roughness = 0.9f}
            );
            // both sides: moonlit outside, firelit inside
            canvasMaterial = scene.Materials().Add(
                MaterialResource{
                    .data =
                        {.albedo = {0.62f, 0.52f, 0.38f},
                         .emissive = CanvasGlow,
                         .roughness = 0.9f},
                    .pipeline = standardMaterial(
                        MaterialDomain::Opaque,
                        RHICullMode::None
                    )
                }
            );

            // the sea's material reads the world's step and the moon from its
            // lanes; casting nothing, it stays out of the shadow's fit
            const auto seaMaterial = scene.Materials().Add(
                MaterialResource{
                    .data = {.custom1 = toVec4(ToMoon, 0.0f)},
                    .pipeline = oceanPipeline()
                }
            );
            const auto seaActor =
                add(scene,
                    seaMaterial,
                    sea,
                    Transform{},
                    {282.0f, 0.4f, 282.0f},
                    PrimitiveFlags::Visible);
            world.Add(
                seaActor,
                ScriptComponent{[seaMaterial](
                                    World& world,
                                    ActorId,
                                    const FrameTime& time
                                ) {
                    auto& materials = world.Service<RenderScene>().Materials();
                    materials.GetRef(seaMaterial).data.custom0.x =
                        static_cast<f32>(stepOf(time) % LoopSteps);
                }}
            );
            // an ellipsoid whose crown is the fire's ground
            add(scene,
                sand,
                island,
                Transform{.position = {0.0f, ISLAND_CENTER_Y, 0.0f}},
                IslandRadii);
            addTipi(scene, wood);

            // the ripples blend over what is drawn before them, so they lead
            effects = std::make_unique<EffectSystem>(Device());
            effects->Add(rainDesc());
            effects->Add(starsDesc());
            effects->Add(meteorsDesc());
            effects->Add(flamesDesc());
            effects->Add(embersDesc());
            world.Provide(*effects);
            addFire(scene, charcoal, coals);
            // the actors' rows land before the first frame records, so the
            // port finds them
            world.Frame(0.0);
            exposures = PortExposures(Port());
            exposures.Expose(
                "camera",
                world.Target<CameraComponent>(cameraActor)
            );
            // a rotation written here lasts until the next mouse look
            exposures.Expose(
                "camera.transform",
                world.TransformTarget(cameraActor)
            );
            // the embers keep to the tipi wherever the fire goes
            exposures.Expose("fire.transform", world.TransformTarget(fire));
        }

        FrameGraphDesc DescribePipeline(
            const StandardFrameConfig& config
        ) override {
            auto desc = describeStandardFrame(config);
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
                    .depth =
                        DepthTargetUse{
                            .target = sceneDepth,
                            .load = RHILoadAction::Load
                        },
                    .kind = HookPassDesc{.hook = EffectsHook}
                }
            );

            return desc;
        }

        void OnProcessInput(const InputProvider& input) override {
            if(fly.ProcessInput(input, makeCamera().lookSensitivity))
                RequestRelativeMouse();
        }

        void OnUpdateScene(f64 deltaTime) override {
            // a still camera writes nothing, so nothing is extracted for it
            if(fly.Moving()) {
                world.SetLocalTransform(
                    cameraActor,
                    fly.Step(
                        world.LocalTransform(cameraActor),
                        makeCamera().moveSpeed,
                        deltaTime
                    )
                );
            }
            world.Frame(deltaTime);
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
                worldStep(),
                world.FrameTicks()
            );
            hooks.push_back(
                PassHook{
                    .name = EffectsHook,
                    .bufferAcquires = releases,
                    .record = [this](
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
                }
            );

            return hooks;
        }

    private:
        static Config makeConfig() {
            return Config{
                .clearColor = {ISLAND_FAR_SEA, 1.0f},
                .drawCapacity = 32,
                .materialCapacity = 16,
                .shadowMapSize = 2048,
                .vertexPoolCapacity = 65536,
                .indexPoolCapacity = 393216
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

        // the sea's own stages: the grid lifted by its waves, shaded per pixel
        static MaterialPipelineDesc oceanPipeline() {
            constexpr CStr Ocean = "Engine/Effects/Sample/Island/Ocean.slang";

            auto desc = standardMaterial(MaterialDomain::Opaque);
            desc.vertexShader = {.path = Ocean, .entryPoint = "vs_ocean"};
            desc.fragmentShader = {.path = Ocean, .entryPoint = "fs_ocean"};

            return desc;
        }

        static MaterialHandle addMaterial(
            RenderScene& scene,
            MaterialData data
        ) {
            return scene.Materials().Add(
                MaterialResource{
                    .data = data,
                    .pipeline = standardMaterial(MaterialDomain::Opaque)
                }
            );
        }

        // the tipi's poles: from the base, through the crossing, and on
        static f32 poleLength() {
            constexpr auto Overhang = 1.0f;

            const auto rise = ISLAND_TIPI_APEX_Y - ISLAND_TIPI_BASE_Y;
            return std::hypot(ISLAND_TIPI_RADIUS, rise) + Overhang;
        }

        // the tripod's logs: from the bed, through the crossing, and on
        static f32 logLength() {
            constexpr auto Overhang = 0.12f;

            const auto rise = ISLAND_LOG_CROSSING_Y - ISLAND_LOG_BASE_Y;
            return std::hypot(ISLAND_LOG_CIRCLE, rise) + Overhang;
        }

        // the rotation that turns the unit `from` onto the unit `to`
        static Vec4 turnTo(Vec3 from, Vec3 to) {
            const auto axis = normalize(cross(from, to));

            return axisAngle(axis, std::acos(dot(from, to)));
        }

        static RHIGraphicsPipelineStateDesc skyPipelineDesc(
            const HookPassFormats& formats
        ) {
            constexpr CStr SkyShader = "Engine/Effects/Sample/Island/Sky.slang";

            RHIGraphicsPipelineStateDesc desc{
                .topology = RHIPrimitiveTopology::TriangleList,
                .vertexShader =
                    RHIShaderDesc{.path = SkyShader, .entryPoint = "vs_sky"},
                .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader =
                    RHIShaderDesc{.path = SkyShader, .entryPoint = "fs_sky"},
                .renderTargetCount = formats.colors.size()
            };
            std::ranges::copy(formats.colors, desc.renderTargetFormats.begin());
            desc.depthStencil = RHIDepthStencilState{
                .format = formats.depth,
                .depthWriteEnable = false,
                .depthFunc = RHIComparisonFunc::LessEqual
            };

            return desc;
        }

        // a mesh at an actor placed by `local` under `parent`; `localHalf` is
        // its half extent around its origin
        ActorId add(
            RenderScene& scene,
            MaterialHandle material,
            const GeometryAllocation& geometry,
            const Transform& local,
            Vec3 localHalf,
            PrimitiveFlags flags = Solid,
            ActorId parent = ActorId::InvalidHandle()
        ) {
            const AABB3D bounds{.center = zeros(), .halfScale = localHalf};
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes =
                        {SubMesh{.geometry = geometry, .localBounds = bounds}},
                    .materials = {material},
                    .localBounds = bounds
                }
            );
            const auto actor = world.Spawn(local, parent);
            world.Add(actor, MeshComponent{.mesh = mesh, .flags = flags});

            return actor;
        }

        void addRod(
            RenderScene& scene,
            MaterialHandle material,
            const GeometryAllocation& geometry,
            Vec3 from,
            Vec3 direction,
            f32 radius,
            f32 length,
            ActorId parent = ActorId::InvalidHandle()
        ) {
            const auto center = from + direction * (0.5f * length);
            add(scene,
                material,
                geometry,
                Transform{
                    .position = center,
                    .rotation = turnTo(unitY(), direction)
                },
                {radius, 0.5f * length, radius},
                Solid,
                parent);
        }

        void addBox(
            RenderScene& scene,
            MaterialHandle material,
            Vec3 position,
            Vec3 scale,
            ActorId parent
        ) {
            add(scene,
                material,
                box,
                Transform{.position = position, .scale = scale},
                ones(),
                Solid,
                parent);
        }

        // the canvas over all but the open facets, and the poles crossing
        // above it
        void addTipi(RenderScene& scene, MaterialHandle wood) {
            constexpr auto Middle =
                0.5f * (ISLAND_TIPI_BASE_Y + ISLAND_TIPI_CANVAS_TOP_Y);
            constexpr auto HalfHeight =
                0.5f * (ISLAND_TIPI_CANVAS_TOP_Y - ISLAND_TIPI_BASE_Y);
            constexpr Vec3 Apex{0.0f, ISLAND_TIPI_APEX_Y, 0.0f};

            add(scene,
                canvasMaterial,
                canvas,
                Transform{.position = {0.0f, Middle, 0.0f}},
                {ISLAND_TIPI_RADIUS, HalfHeight, ISLAND_TIPI_RADIUS});
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

        // the glowing bed, three logs leaning into a tripod over it, the light
        // and the flames and embers, all placed from the fire's actor
        void addFire(
            RenderScene& scene,
            MaterialHandle charcoal,
            MaterialHandle coals
        ) {
            constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;
            constexpr auto BedHalfHeight = 0.03f;
            constexpr Vec3 Crossing{
                0.0f,
                ISLAND_LOG_CROSSING_Y - FirePosition.y,
                0.0f
            };

            fire = world.Spawn(Transform{.position = FirePosition});
            addBox(
                scene,
                coals,
                {0.0f,
                 ISLAND_COALS_TOP_Y - BedHalfHeight - FirePosition.y,
                 0.0f},
                {ISLAND_COALS_HALF, BedHalfHeight, ISLAND_COALS_HALF},
                fire
            );
            for(u32 i = 0; i < 3; ++i) {
                const auto azimuth = TwoPi * static_cast<f32>(i) / 3.0f;
                const Vec3 base{
                    ISLAND_LOG_CIRCLE * std::sin(azimuth),
                    ISLAND_LOG_BASE_Y - FirePosition.y,
                    ISLAND_LOG_CIRCLE * std::cos(azimuth)
                };
                addRod(
                    scene,
                    charcoal,
                    log,
                    base,
                    normalize(Crossing - base),
                    LogRadius,
                    logLength(),
                    fire
                );
            }
            // where the effects' own descriptions put them
            for(const auto* name: {"flames", "embers"}) {
                const auto origin = effects->Origin(name);
                world.Add(
                    world.Spawn(
                        Transform{.position = origin - FirePosition},
                        fire
                    ),
                    EffectComponent{name}
                );
            }

            // the fire breathes on the world's loop, and its glow through the
            // canvas with it
            const Vec3 lightPosition{ISLAND_FIRE_LIGHT};
            const auto light = world.Spawn(
                Transform{.position = lightPosition - FirePosition},
                fire
            );
            world.Add(
                light,
                LightComponent{
                    .kind = LightKind::Point,
                    .color = {1.0f, 0.55f, 0.2f},
                    .intensity = FireIntensity,
                    .range = 6.5f
                }
            );
            world.Add(
                light,
                ScriptComponent{[canvas = canvasMaterial](
                                    World& world,
                                    ActorId self,
                                    const FrameTime& time
                                ) {
                    const auto breath = fireBreath(stepOf(time));
                    world.Write<LightComponent>(self)->intensity =
                        FireIntensity * breath;
                    auto& materials = world.Service<RenderScene>().Materials();
                    materials.GetRef(canvas).data.emissive =
                        CanvasGlow * breath;
                }}
            );
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

        // the step the effects and the scene share; the loops divide its wrap
        static u32 stepOf(const FrameTime& time) {
            return static_cast<u32>(time.tick);
        }
        u32 worldStep() const { return static_cast<u32>(world.Ticks()); }
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
