#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "EffectSystem.hpp"
#include "FlyCamera.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "ParticleEffects.hpp"
#include "RenderApp.hpp"
#include "StandardPipeline.hpp"
#include "WorldClock.hpp"

namespace Crowy
{
    // A night island whose effects draw in a hook pass before the glass, one
    // step a recorded frame, so frame 60 is the same picture in every run.
    class Island final: public RenderApp {
        static constexpr CStr EffectsHook = "effects";
        // where the campfire burns, on the island's crown
        static constexpr Vec3 FirePosition{0.0f, 0.4f, 0.0f};
        static constexpr f32 FireIntensity = 7.0f;

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
            // the shadow map fits every visible bound, so the sea stays small
            const auto seaMesh = MakePlane(Vec2{30.0f, 30.0f});
            const auto islandMesh = MakeSphere(1.0f, 48, 24);
            const auto boxMesh = MakeBox(1.0f);

            sea = pool.Add(seaMesh.vertices, seaMesh.indices);
            island = pool.Add(islandMesh.vertices, islandMesh.indices);
            box = pool.Add(boxMesh.vertices, boxMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            scene.Lights().Add(
                LightSnapshot{
                    .castShadow = true,
                    .color = {0.55f, 0.65f, 1.0f},
                    .intensity = 0.35f,
                    .direction = normalize(Vec3{0.35f, -1.0f, 0.55f})
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

            add(scene, water, sea, zeros(), {30.0f, 0.001f, 30.0f});
            // a flattened sphere whose crown is the fire's ground
            add(
                scene,
                sand,
                island,
                {0.0f, -1.2f, 0.0f},
                ones(),
                {9.0f, 1.6f, 7.0f}
            );
            // the hut, behind and left of the fire
            add(
                scene,
                wood,
                box,
                {-3.2f, 1.1f, 2.6f},
                ones(),
                {1.3f, 1.0f, 1.1f}
            );
            add(
                scene,
                charcoal,
                box,
                {-3.2f, 2.15f, 2.6f},
                ones(),
                {1.5f, 0.08f, 1.3f}
            );
            // two crossed logs and the glowing heart between them
            add(
                scene,
                charcoal,
                box,
                FirePosition + Vec3{0.0f, 0.06f, 0.0f},
                ones(),
                {0.5f, 0.06f, 0.07f}
            );
            add(
                scene,
                charcoal,
                box,
                FirePosition + Vec3{0.0f, 0.13f, 0.0f},
                ones(),
                {0.07f, 0.06f, 0.5f}
            );
            add(
                scene,
                embers,
                box,
                FirePosition + Vec3{0.0f, 0.1f, 0.0f},
                ones(),
                {0.16f, 0.08f, 0.16f}
            );

            effects = std::make_unique<EffectSystem>(Device());
            effects->Add(rainDesc());
            effects->Add(embersDesc());
            effects->Add(
                ParticleEffectDesc{
                    .name = "meteors",
                    .shader = "Engine/Effects/Sample/Island/Meteors.slang",
                    .count = 24,
                    .seed = 41,
                    .prewarmSteps = 240,
                    .emitter = Vec4{0.0f, 0.0f, 0.0f, 90.0f},
                    .params = {Vec4{70.0f, 0.12f, 6.0f, 0.0f}, Vec4{}, Vec4{}},
                    .draws =
                        {{.entry = "meteors", .blend = EffectBlend::Additive}}
                }
            );
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
                        return effects->Draw(
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
                .clearColor = {0.015f, 0.02f, 0.045f, 1.0f},
                .drawCapacity = 32,
                .materialCapacity = 16,
                .shadowMapSize = 2048,
                .vertexPoolCapacity = 4096,
                .indexPoolCapacity = 16384
            };
        }

        // a few strides from the fire, a little above it
        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.6f, 1.7f, -5.5f},
                .pitch = 0.14f,
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 200.0f
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

        // a unit mesh at `position`, stretched by `scale`; `localHalf` is the
        // unit mesh's half extent
        static void add(
            RenderScene& scene,
            MaterialHandle material,
            const GeometryAllocation& geometry,
            Vec3 position,
            Vec3 localHalf,
            Vec3 scale = ones()
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
                    .localToWorld = translateMat(position) * scaleMat(scale),
                    .worldBounds =
                        AABB3D{
                            .center = position,
                            .halfScale = localHalf * scale
                        },
                    .mesh = mesh
                }
            );
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
