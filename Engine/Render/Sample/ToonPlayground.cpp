#include <algorithm>
#include <format>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

#include "PlaygroundScene.hpp"
#include "ShadingModel.hpp"
#include "StandardPipeline.hpp"

namespace Crowy
{
    namespace
    {
        constexpr CStr NormalsShader =
            "Engine/Render/Sample/NormalsPrepass.slang";
        constexpr CStr OutlineShader = "Engine/Render/Sample/Outline.slang";
        constexpr CStr PosterizeShader = "Engine/Render/Sample/Posterize.slang";

        // a jump in depth relative to the view depth, and the length of a
        // jump between unit normals, past which the outline draws
        constexpr f32 OutlineDepthThreshold = 0.05f;
        constexpr f32 OutlineNormalThreshold = 0.4f;
        // the steps each display channel snaps to
        constexpr f32 PosterizeLevels = 6.0f;

        FrameTargetID targetNamed(const FramePipelineDesc& desc, StrView name) {
            const auto found =
                std::ranges::find(desc.targets, name, &FrameTargetDesc::name);
            if(found == desc.targets.end()) {
                throw std::invalid_argument(
                    std::format("the pipeline has no target '{}'", name)
                );
            }

            // targets[i] is ID i + 1
            return static_cast<FrameTargetID>(found - desc.targets.begin() + 1);
        }

        PassDescs::iterator passNamed(FramePipelineDesc& desc, StrView name) {
            const auto found =
                std::ranges::find(desc.passes, name, &PassDesc::name);
            if(found == desc.passes.end()) {
                throw std::invalid_argument(
                    std::format("the pipeline has no pass '{}'", name)
                );
            }

            return found;
        }

        // the tone map, then the posterize on its display values; the UI
        // rides the posterize
        std::vector<PostPassDesc> toonPost() {
            return {
                tonemapPass(),
                PostPassDesc{
                    .name = "Posterize",
                    .fragmentShader =
                        {.path = PosterizeShader, .entryPoint = "fs_main"},
                    .params = {PosterizeLevels, 0.0f, 0.0f, 0.0f}
                }
            };
        }

        // Multiplies its lines into the scene colour, so a line keeps the
        // hue under it and the pass never reads what it writes; the
        // destination's alpha is kept.
        PassDesc outlinePass(
            FrameTargetID sceneColor,
            FrameTargetID sceneDepth,
            FrameTargetID normals,
            f32 nearZ,
            f32 farZ
        ) {
            RHIBlendState multiply{};
            multiply.renderTargets[0] = RHIRenderTargetBlendState{
                .blendEnable = true,
                .srcBlend = RHIBlend::Zero,
                .dstBlend = RHIBlend::SrcColor,
                .srcBlendAlpha = RHIBlend::Zero,
                .dstBlendAlpha = RHIBlend::One
            };

            return PassDesc{
                .name = "Outline",
                .colors = {ColorTargetUse{
                    .target = sceneColor,
                    .load = RHILoadAction::Load
                }},
                .reads = {sceneDepth, normals},
                .kind = FullscreenPassDesc{
                    .fragmentShader =
                        {.path = OutlineShader, .entryPoint = "fs_outline"},
                    .blend = multiply,
                    .params = {
                        OutlineDepthThreshold,
                        OutlineNormalThreshold,
                        nearZ,
                        farZ
                    }
                }
            };
        }

        // links Toon and writes its lanes; its terminator band is as wide as
        // the roughness, so a hard terminator is a low roughness
        void restyle(
            RenderScene& scene,
            MaterialHandle handle,
            const ToonLanes& lanes,
            std::optional<f32> roughness = std::nullopt
        ) {
            auto& material = scene.Materials().GetRef(handle);
            material.pipeline.shadingModule = ToonShadingModule;
            material.data.custom0 = toonCustom0(lanes);
            material.data.custom1 = toonCustom1(lanes);
            if(roughness)
                material.data.roughness = *roughness;
        }
    }

    // Playground's map under a second pipeline, composed here from the
    // engine's public pieces: a prepass that also writes normals, Toon and
    // Unlit materials, an outline between the opaque and the translucent
    // pass, and a posterize after the tone map.
    class ToonPlayground final: public PlaygroundScene {
    public:
        ToonPlayground()
            : PlaygroundScene(makeConfig()) {}

    protected:
        FramePipelineDesc DescribePipeline(
            const StandardPipelineConfig& config
        ) override {
            auto desc = makeStandardPipeline(config);
            // a data view, a wireframe, Overdraw or debug.depthPrepass off:
            // the standard list, as Playground draws it
            if(!config.depthPrepass || config.post != toonPost())
                return desc;

            const auto sceneDepth = targetNamed(desc, "SceneDepth");
            // cleared to a zero vector, encoded
            desc.targets.push_back(
                FrameTargetDesc{
                    .name = "Normals",
                    .format = RHIPixelFormat::RGBA8_UNORM,
                    .size = desc.targets[sceneDepth - 1].size,
                    .clearColor = {0.5f, 0.5f, 0.5f, 0.0f}
                }
            );
            const auto normals =
                static_cast<FrameTargetID>(desc.targets.size());

            // the same depths from the same vertex stage, plus the normal
            auto& prepass = *passNamed(desc, "DepthPrepass");
            prepass.name = "NormalsPrepass";
            prepass.colors = {ColorTargetUse{.target = normals}};
            auto& state = std::get<MeshPassDesc>(prepass.kind).state;
            state.fragmentShader = RHIShaderDesc{
                .path = NormalsShader,
                .entryPoint = "fs_normals"
            };
            state.linksShading = false;

            // before the glass, so glass gets no lines and what stands
            // behind it keeps its own
            const auto camera = MakeCamera();
            desc.passes.insert(
                passNamed(desc, "Translucent"),
                outlinePass(
                    desc.sceneColor,
                    sceneDepth,
                    normals,
                    camera.nearZ,
                    camera.farZ
                )
            );

            return desc;
        }

        // every material Toon but the emissive panels, which are Unlit
        void OnRestyleMaterials(
            RenderScene& scene,
            const PlaygroundMaterials& materials,
            PlaygroundShading& shading
        ) override {
            // warm bases under a cool blue shadow, no highlight, no rim
            constexpr ToonLanes Room{
                .highlightSize = 0.0f,
                .rimStrength = 0.0f,
                .rimWidth = 0.0f,
                .shadeTint = {0.50f, 0.56f, 0.78f}
            };
            restyle(scene, materials.floor, Room, 0.15f);
            restyle(scene, materials.wall, Room, 0.15f);
            restyle(
                scene,
                materials.block,
                ToonLanes{
                    .highlightSize = 0.05f,
                    .rimStrength = 0.3f,
                    .rimWidth = 0.25f,
                    .shadeTint = {0.48f, 0.50f, 0.75f}
                },
                0.15f
            );

            for(const auto panel: materials.emissive) {
                scene.Materials().GetRef(panel).pipeline.shadingModule =
                    UnlitShadingModule;
            }

            // the chart keeps its roughness row, so the band widens from
            // left to right; the metals carry a larger disc and a stronger
            // rim over a hue-shifted shade
            constexpr ToonLanes Dielectric{
                .highlightSize = 0.06f,
                .rimStrength = 0.3f,
                .rimWidth = 0.30f,
                .shadeTint = {0.55f, 0.35f, 0.60f}
            };
            constexpr ToonLanes Metal{
                .highlightSize = 0.18f,
                .rimStrength = 0.8f,
                .rimWidth = 0.35f,
                .shadeTint = {0.50f, 0.35f, 0.65f}
            };
            for(u32 i = 0; i < materials.chart.size(); ++i) {
                restyle(
                    scene,
                    materials.chart[i],
                    i < PlaygroundChartColumns ? Dielectric : Metal
                );
            }
            shading.chart = ChartShading::Toon;

            for(const auto bar: materials.bars) {
                restyle(
                    scene,
                    bar,
                    ToonLanes{
                        .highlightSize = 0.08f,
                        .rimStrength = 0.4f,
                        .rimWidth = 0.30f,
                        .shadeTint = {0.50f, 0.50f, 0.75f}
                    },
                    0.2f
                );
            }
            // a rim on the glass
            restyle(
                scene,
                materials.translucent,
                ToonLanes{
                    .highlightSize = 0.15f,
                    .rimStrength = 0.8f,
                    .rimWidth = 0.40f,
                    .shadeTint = {0.60f, 0.70f, 0.90f}
                },
                0.2f
            );
            restyle(
                scene,
                materials.doubleSided,
                ToonLanes{
                    .highlightSize = 0.0f,
                    .rimStrength = 0.3f,
                    .rimWidth = 0.30f,
                    .shadeTint = {0.45f, 0.60f, 0.55f}
                },
                0.2f
            );
        }

    private:
        static Config makeConfig() {
            auto config = MakeConfig();
            config.post = toonPost();

            return config;
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "ToonPlayground",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<ToonPlayground>(argc, argv, windowConfig);
}
