#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "EffectSystem.hpp"
#include "EnumUtil.hpp"
#include "FlyCamera.hpp"
#include "Geometry/Overlap3D.hpp"
#include "Island/IslandScene.h"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "ParticleEffects.hpp"
#include "PipelineCache.hpp"
#include "RenderApp.hpp"
#include "StandardPipeline.hpp"
#include "WorldClock.hpp"

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
        // where the campfire burns, on the island's crown
        static constexpr Vec3 FirePosition{0.0f, 0.4f, 0.0f};
        static constexpr f32 FireIntensity = 7.0f;
        static constexpr Vec3 IslandRadii{ISLAND_RADII};
        static constexpr Vec3 ToMoon{ISLAND_TO_MOON};

        GeometryAllocation sea{};
        GeometryAllocation island{};
        GeometryAllocation box{};
        LightHandle fire{};
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

            sea = pool.Add(seaMesh.vertices, seaMesh.indices);
            island = pool.Add(islandMesh.vertices, islandMesh.indices);
            box = pool.Add(boxMesh.vertices, boxMesh.indices);
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
                    .position = FirePosition + Vec3{0.0f, 0.4f, 0.0f},
                    .range = 9.0f
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
            const auto embers = addMaterial(
                scene,
                {
                    .albedo = {0.1f, 0.05f, 0.02f},
                    .emissive = {6.0f, 2.2f, 0.5f},
                    .roughness = 0.9f
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
            // the hut, behind and left of the fire
            addBox(scene, wood, {-3.2f, 1.1f, 2.6f}, {1.3f, 1.0f, 1.1f});
            addBox(scene, charcoal, {-3.2f, 2.15f, 2.6f}, {1.5f, 0.08f, 1.3f});
            // two crossed logs and the glowing heart between them
            addBox(
                scene,
                charcoal,
                FirePosition + Vec3{0.0f, 0.06f, 0.0f},
                {0.5f, 0.06f, 0.07f}
            );
            addBox(
                scene,
                charcoal,
                FirePosition + Vec3{0.0f, 0.13f, 0.0f},
                {0.07f, 0.06f, 0.5f}
            );
            addBox(
                scene,
                embers,
                FirePosition + Vec3{0.0f, 0.1f, 0.0f},
                {0.16f, 0.08f, 0.16f}
            );

            // the ripples blend over what is drawn before them, so they lead
            effects = std::make_unique<EffectSystem>(Device());
            effects->Add(rainDesc());
            effects->Add(starsDesc());
            effects->Add(meteorsDesc());
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

        // the fire breathes on the world's loop, never on the clock
        void OnUpdateScene(f64) override {
            constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;

            const auto step = worldStep();
            const auto breath =
                1.0f + 0.12f * std::sin(TwoPi * loopPhase(step, 241)) +
                0.06f * std::sin(TwoPi * loopPhase(step, 854));
            Scene().Lights().GetRef(fire).intensity = FireIntensity * breath;
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

        // over the shallows, looking a little up: the fire ahead, the moon
        // and its path on the water to the right
        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.3f, 1.8f, -9.5f},
                .pitch = -0.04f,
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 320.0f
            };
        }

        // a few hundred sparks off the fire's heart, a second or two each
        static ParticleEffectDesc embersDesc() {
            return ParticleEffectDesc{
                .name = "embers",
                .shader = "Engine/Effects/Sample/Island/Embers.slang",
                .count = 384,
                .seed = 11,
                .prewarmSteps = 240,
                .emitter =
                    toVec4(FirePosition + Vec3{0.0f, 0.12f, 0.0f}, 0.22f),
                .params =
                    {Vec4{0.35f, 0.9f, 0.55f, 1.1f},
                     Vec4{1.6f, 0.018f, 24.0f, 0.0f},
                     Vec4{}},
                .draws = {{.entry = "embers", .blend = EffectBlend::Additive}}
            };
        }

        // a dome of stars that follows the camera, a few hundred in frame
        static ParticleEffectDesc starsDesc() {
            return ParticleEffectDesc{
                .name = "stars",
                .shader = "Engine/Effects/Sample/Island/Stars.slang",
                .count = 2560,
                .seed = 53,
                .emitter = Vec4{0.0f, 0.0f, 0.0f, 170.0f},
                .params =
                    {Vec4{0.0349f, 1.5f, 8.0f, 0.0f},
                     Vec4{0.1f, 2.2f, 7.0f, 0.0f},
                     Vec4{}},
                .draws = {{.entry = "stars", .blend = EffectBlend::Additive}}
            };
        }

        // a shower from a radiant low on the left, about one in the sky at a
        // time; the seed puts one high on the left at frame 60
        static ParticleEffectDesc meteorsDesc() {
            return ParticleEffectDesc{
                .name = "meteors",
                .shader = "Engine/Effects/Sample/Island/Meteors.slang",
                .count = 12,
                .seed = 1,
                .emitter = Vec4{-1.6581f, 0.0873f, 160.0f, 0.0f},
                .params =
                    {Vec4{-0.7854f, 0.6109f, 0.1745f, 0.4189f},
                     Vec4{0.262f, 0.524f, 0.25f, 2.0f},
                     Vec4{1.2f, 0.125f, 0.0f, 0.0f}},
                .draws =
                    {{.entry = "meteors", .blend = EffectBlend::Additive}}
            };
        }

        // a shower over the fire and the shore; the island's ellipsoid is
        // where its drops land
        static ParticleEffectDesc rainDesc() {
            return ParticleEffectDesc{
                .name = "rain",
                .shader = "Engine/Effects/Sample/Island/Rain.slang",
                .count = 3000,
                .seed = 23,
                .prewarmSteps = 120,
                .emitter = Vec4{0.0f, 6.5f, 2.0f, 6.0f},
                .params =
                    {Vec4{8.0f, 8.0f, 9.0f, 1.2f},
                     Vec4{0.4f, 0.03f, 0.35f, 0.28f},
                     Vec4{9.0f, 1.6f, 7.0f, -1.2f}},
                .draws =
                    {{.entry = "ripples", .blend = EffectBlend::Alpha},
                     {.entry = "streaks", .blend = EffectBlend::Additive}}
            };
        }

        static MaterialPipelineDesc opaquePipeline() {
            constexpr CStr StandardForward =
                "Engine/Render/Shader/StandardForward.slang";

            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = StandardForward, .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = StandardForward, .entryPoint = "fs_opaque"},
                .rasterizer = {.frontCounterClockwise = false},
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

        // a unit sphere stretched to `radii` with the stretch's own normals:
        // the vertex stage turns a normal by the world's rotation alone
        static MeshData makeEllipsoid(Vec3 radii) {
            auto mesh = MakeSphere(1.0f, 48, 24);
            for(auto& vertex: mesh.vertices) {
                const auto unit = vertex.position;
                const auto normal = normalize(
                    Vec3{unit.x / radii.x, unit.y / radii.y, unit.z / radii.z}
                );
                const auto tangent =
                    static_cast<Vec3>(vertex.tangent) * radii;
                vertex.position = unit * radii;
                vertex.normal = normal;
                vertex.tangent = toVec4(
                    normalize(tangent - normal * dot(normal, tangent)),
                    vertex.tangent.w
                );
            }

            return mesh;
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

        // the night sky first, where the scene left the depth clear
        void drawSky(RHICommandList& cmdList, const HookPassContext& context) {
            // the window main opens
            constexpr auto Aspect = 1280.0f / 720.0f;

            const auto view = Camera().View();
            const auto projection = Camera().Projection(Aspect);
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

        // one step a frame, the clock the effects, the light and the sea share
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
