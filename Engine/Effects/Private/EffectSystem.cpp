#include "EffectSystem.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    static_assert(sizeof(EffectPush) <= RHI_PUSH_CONSTANT_BYTES);

    namespace
    {
        RHIBlendState blendOf(EffectBlend blend) {
            RHIBlendState state{};
            auto& target = state.renderTargets[0];
            target.blendEnable = true;
            target.srcBlendAlpha = RHIBlend::Zero;
            target.dstBlendAlpha = RHIBlend::One;
            if(blend == EffectBlend::Additive) {
                target.srcBlend = RHIBlend::One;
                target.dstBlend = RHIBlend::One;
            } else {
                target.srcBlend = RHIBlend::SrcAlpha;
                target.dstBlend = RHIBlend::InvSrcAlpha;
            }

            return state;
        }

        // a strip per particle, depth-tested against the scene and never
        // writing it
        RHIGraphicsPipelineStateDesc drawPipelineDesc(
            const ParticleEffectDesc& effect,
            const EffectDrawDesc& draw,
            const HookPassFormats& formats
        ) {
            RHIGraphicsPipelineStateDesc desc{
                .preRasterizer =
                    RHILegacyFrontendDesc{
                        .topology = RHIPrimitiveTopology::TriangleStrip,
                        .vertexShader =
                            RHIShaderDesc{
                                .path = effect.shader,
                                .entryPoint = "vs_" + draw.entry
                            }
                    },
                .rasterizer =
                    RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader =
                    RHIShaderDesc{
                        .path = effect.shader,
                        .entryPoint = "fs_" + draw.entry
                    },
                .blend = blendOf(draw.blend),
                .renderTargetCount = formats.colors.size(),
                .profile = "sm_6_8"
            };
            std::ranges::copy(formats.colors, desc.renderTargetFormats.begin());
            if(formats.depth != RHIPixelFormat::Unknown) {
                desc.depthStencil = RHIDepthStencilState{
                    .format = formats.depth,
                    .depthWriteEnable = false,
                    .depthFunc = RHIComparisonFunc::Less
                };
            }

            return desc;
        }

        std::unique_ptr<ComputeKernel> stepKernel(
            RHIDevice& device,
            const ParticleEffectDesc& desc
        ) {
            return std::make_unique<ComputeKernel>(
                device,
                RHIShaderDesc{.path = desc.shader, .entryPoint = "cs_step"}
            );
        }

        [[noreturn]] void refuse(StrView effect, StrView rule) {
            throw std::invalid_argument(
                std::format("effect '{}': {}", effect, rule)
            );
        }
    }

    EffectSystem::Effect::Effect(
        RHIDevice& device,
        ParticleEffectDesc desc,
        RHIResourceUsage reader
    )
        : desc(std::move(desc)),
          particles(
              device,
              this->desc.count,
              sizeof(EffectParticle),
              this->desc.name,
              reader
          ),
          step(stepKernel(device, this->desc)) {}

    EffectSystem::~EffectSystem() = default;

    EffectSystem::EffectSystem(RHIDevice& device, RHIResourceUsage reader)
        : device(device),
          kernels(device),
          reader(reader) {}

    void EffectSystem::Add(ParticleEffectDesc desc) {
        if(desc.name.empty())
            refuse(desc.name, "an effect is named");
        if(desc.count == 0 || desc.count > MaxDispatchThreads) {
            refuse(
                desc.name,
                std::format(
                    "an effect has 1 to {} particles, not {}",
                    MaxDispatchThreads,
                    desc.count
                )
            );
        }
        const auto named = [&](const auto& effect) {
            return effect->desc.name == desc.name;
        };
        if(std::ranges::any_of(effects, named))
            refuse(desc.name, "two effects have this name");
        for(const auto& draw: desc.draws) {
            if(draw.entry.empty())
                refuse(desc.name, "a draw names its entries");
        }

        auto effect = std::make_unique<Effect>(device, std::move(desc), reader);
        fields.push_back(&effect->particles);
        effects.push_back(std::move(effect));
    }

    std::span<const RHIBufferBarrier> EffectSystem::Simulate(
        RHICommandList& cmdList,
        const EffectView& view,
        u32 worldStep
    ) {
        this->view = view;
        this->worldStep = worldStep;
        const auto started = [](const auto& effect) {
            return effect->next > 0;
        };
        if(effects.empty() || (paused && std::ranges::all_of(effects, started)))
            return {};

        FieldPass pass(cmdList, kernels);
        pass.Begin(fields);
        for(auto& effect: effects) {
            const auto& desc = effect->desc;
            FieldBuffer* const touches[] = {&effect->particles};
            // the first frame shows the steady state the prewarm reaches;
            // paused, only an effect not yet started runs
            auto steps = paused ? 0u : 1u;
            if(effect->next == 0)
                steps = desc.prewarmSteps + 1;
            for(u32 s = 0; s < steps; ++s) {
                auto push = pushOf(*effect);
                push.step = effect->next++;
                push.worldStep = worldStep - (steps - 1 - s);
                pass.Dispatch(
                    *effect->step,
                    push,
                    Size3D{desc.count, 1, 1},
                    touches
                );
            }
        }

        const auto ended = pass.End();
        releases.assign(ended.begin(), ended.end());

        return releases;
    }

    u32 EffectSystem::Draw(
        RHICommandList& cmdList,
        const HookPassContext& context,
        PipelineCache& pipelines
    ) {
        u32 draws = 0;
        for(auto& effect: effects) {
            // nothing has written its particles yet
            if(effect->next == 0)
                continue;

            const auto& desc = effect->desc;
            auto push = pushOf(*effect);
            push.step = effect->next - 1;
            for(const auto& draw: desc.draws) {
                cmdList.SetPipelineState(
                    pipelines.Resolve(
                        drawPipelineDesc(desc, draw, context.formats)
                    )
                );
                cmdList.SetPushGraphicsConstants(push);
                cmdList.Draw(draw.verticesPerInstance, desc.count);
                ++draws;
            }
        }

        return draws;
    }

    usize EffectSystem::ReloadKernels() {
        Kernels built;
        for(const auto& effect: effects)
            built.push_back(stepKernel(device, effect->desc));

        for(usize i = 0; i < effects.size(); ++i) {
            std::swap(effects[i]->step, built[i]);
            device.DeferRetire([old = std::move(built[i])]() mutable {
                old.reset();
            });
        }

        return effects.size();
    }

    FieldBuffer& EffectSystem::Particles(StrView name) {
        return find(name).particles;
    }

    u32 EffectSystem::Steps(StrView name) const {
        return find(name).next;
    }

    EffectPush EffectSystem::pushOf(Effect& effect) const {
        const auto& desc = effect.desc;

        return EffectPush{
            .particlesRW = effect.particles.Writable(),
            .particles = effect.particles.Readable(),
            .count = desc.count,
            .dt = EffectStep,
            .seed = desc.seed,
            .cameraRight = toVec4(view.right, 0.0f),
            .cameraUp = toVec4(view.up, 0.0f),
            .emitter = desc.emitter,
            .params = desc.params,
            .worldStep = worldStep
        };
    }

    EffectSystem::Effect& EffectSystem::find(StrView name) const {
        const auto found =
            std::ranges::find(effects, name, [](const auto& effect) {
                return StrView{effect->desc.name};
            });
        if(found == effects.end())
            refuse(name, "no effect has this name");

        return **found;
    }
}
