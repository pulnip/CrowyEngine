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

    RenderPassBinderRegistry makeRenderPassBinderRegistry(){
        RenderPassBinderRegistry reg;
        reg.emplace("shader", std::make_unique<ShaderBinder>());

        return reg;
    }
}