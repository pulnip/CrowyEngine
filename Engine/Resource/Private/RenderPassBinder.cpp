#include "RenderPassBinder.hpp"

namespace Crowy
{
    static CBufferFieldType toCBufferFieldType(std::string_view str){
        static std::unordered_map<std::string, CBufferFieldType,
            StringHash, std::equal_to<>
        > text2FieldType = {
            {     "INT", CBufferFieldType::Int32   },
            {   "INT32", CBufferFieldType::Int32   },
            {   "FLOAT", CBufferFieldType::Float   },
            {  "FLOAT2", CBufferFieldType::Float2  },
            {  "FLOAT3", CBufferFieldType::Float3  },
            {  "FLOAT4", CBufferFieldType::Float4  },
            {"FLOAT4X4", CBufferFieldType::Float4x4}
        };
        auto upper = toUpper(str);

        auto it = text2FieldType.find(upper);
        if(it == text2FieldType.end()){
            return CBufferFieldType::Unknown;
        }
        return it->second;
    }

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

    void ShaderBinder::freeze(std::vector<RenderPassSpec>& passes, RenderPassElementBindPlan& plan){
        for(const auto& p: plan.shaders){
            auto& pass = passes[p.index];

            pass.shader = p.spec;
        }
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

    void SamplerBinder::freeze(std::vector<RenderPassSpec>& passes, RenderPassElementBindPlan& plan){
        for(const auto& p: plan.samplers){
            auto& pass = passes[p.index];

            pass.fs_samplers = p.spec;
        }
    }

    void readCBufferData(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        CBufferSpec& cbuffer
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return;

        if(auto arr = std::get_if<VArray>(n)){
            for(size_t i: arr->elements){
                auto field = std::get_if<VTable>(&arena.nodes[i]);
                if(!field)
                    continue;

                auto name = readString(arena, *field, errors, "name");
                auto type = readString(arena, *field, errors, "type");

                if(!name || !type)
                    // TODO. field without name and type is error
                    continue;;

                auto t = toCBufferFieldType(*type);
                switch(t){
                case CBufferFieldType::Unknown:
                    continue;
                case CBufferFieldType::Int32: {
                    auto proxy = cbuffer.newField(*name, t);
                    if(auto data = readFloat(arena, *field, errors, "data"))
                        proxy = static_cast<int32_t>(*data);
                } break;
                case CBufferFieldType::Float: {
                    auto proxy = cbuffer.newField(*name, t);
                    if(auto data = readFloat(arena, *field, errors, "data"))
                        proxy = static_cast<float>(*data);
                } break;
                case CBufferFieldType::Float2: {
                    auto proxy = cbuffer.newField(*name, t);
                    if(auto data = readVec2(arena, *field, errors, "data"))
                        proxy = *data;
                } break;
                case CBufferFieldType::Float3: {
                    auto proxy = cbuffer.newField(*name, t);
                    if(auto data = readVec3(arena, *field, errors, "data"))
                        proxy = *data;
                } break;
                case CBufferFieldType::Float4: {
                    auto proxy = cbuffer.newField(*name, t);
                    if(auto data = readVec4(arena, *field, errors, "data"))
                        proxy = *data;
                } break;
                case CBufferFieldType::Float4x4: {
                    auto proxy = cbuffer.newField(*name, t);
                    if(auto data = readMat4(arena, *field, errors, "data"))
                        proxy = *data;
                } break;
                default:
                    std::unreachable();
                }
            }
        }
    }

    void CBufferBinder::validateAndPlanArray(const ValueArena& arena,
        const VArray& src, size_t index, RenderPassElementBindPlan& plan
    ){
        std::vector<CBufferSpec> cbufferSpec;

        for(size_t i: src.elements){
            auto table = std::get_if<VTable>(&arena.nodes[i]);
            if(!table)
                continue;

            auto name = readString(arena, *table, plan.errors, "name");
            auto slot = readFloat(arena, *table, plan.errors, "slot");
            if(!name || !slot)
                continue;

            CBufferSpec cbuffer{
                .name = *name,
                .slot = static_cast<uint32_t>(*slot)
            };
            readCBufferData(arena, *table, plan.errors, "value", cbuffer);

            cbufferSpec.push_back(std::move(cbuffer));
        }

        plan.cbuffers.push_back({
            .spec = cbufferSpec,
            .index = index,
            .location = src.location
        });
    }

    void CBufferBinder::freeze(std::vector<RenderPassSpec>& passes, RenderPassElementBindPlan& plan){
        for(const auto& p: plan.cbuffers){
            auto& pass = passes[p.index];

            pass.fs_cbuffers = p.spec;
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
        reg.emplace("fs_cbuffer", std::make_unique<CBufferBinder>());

        return reg;
    }
}