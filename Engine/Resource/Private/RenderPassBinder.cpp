#include "RenderPassBinder.hpp"

namespace Crowy
{
    void ShaderBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t index, RenderPassElementBindPlan& plan
    ){
        auto vsFile = readString(arena, src, plan.errors, "vs_file");
        auto vsFunc = readString(arena, src, plan.errors, "vs_func", "vertex_main");
        auto fsFile = readString(arena, src, plan.errors, "fs_file");
        auto fsFunc = readString(arena, src, plan.errors, "fs_func", "fragment_main");

        if(!vsFile || !fsFile)
            return;

        plan.shaders.push_back({
            .spec = ShaderSpec{
                .vsFilePath = *vsFile,
                .vsFuncName = vsFunc,
                .fsFilePath = *fsFile,
                .fsFuncName = fsFunc,
            },
            .index = index,
            .location = src.location
        });
    }

    void ShaderBinder::freeze(RenderSpec& spec, RenderPassElementBindPlan& plan){
        for(const auto& p: plan.shaders){
            auto& pass = spec.passes[p.index];

            pass.shader = p.spec;
        }
    }

    void SamplerBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t index, RenderPassElementBindPlan& plan
    ){
        // No-op
    }

    void SamplerBinder::validateAndPlanArray(const ValueArena& arena,
        const VArray& src, size_t index, RenderPassElementBindPlan& plan
    ){
        std::vector<RHISamplerState> samplerSpec;

        for(size_t i: src.elements){
            auto table = std::get_if<VTable>(&arena.nodes[i]);
            if(!table)
                continue;

            auto presetName = readString(arena, *table, plan.errors, "preset");

            if(presetName.has_value()){
                if(auto it = presets.find(*presetName); it != presets.end()){
                    samplerSpec.push_back(it->second);
                }
                else{
                    // preset not exists
                }
            }
            else{
                // manually parse sampler option
            }
        }

        plan.samplers.push_back({
            .spec = samplerSpec,
            .index = index,
            .location = src.location
        });
    }

    void SamplerBinder::freeze(RenderSpec& spec, RenderPassElementBindPlan& plan){
        for(const auto& p: plan.samplers){
            auto& pass = spec.passes[p.index];

            pass.fs_samplers = p.spec;
        }
    }

    RenderPassBinderRegistry makeRenderPassBinderRegistry(const SamplerPresets& presets){
        RenderPassBinderRegistry reg;
    #ifdef CROWY_METALRHI
        reg.emplace("metal_shader", std::make_unique<ShaderBinder>());
    #elifdef CROWY_D3DRHI
        reg.emplace("d3d_shader", std::make_unique<ShaderBinder>());
    #endif
        reg.emplace("fs_samplers", std::make_unique<SamplerBinder>(presets));

        return reg;
    }
}