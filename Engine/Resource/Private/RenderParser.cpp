#include <cstdint>
#include <optional>
#include <toml++/toml.hpp>
#include <unordered_map>
#include "ParseResult.hpp"
#include "RenderSpec.hpp"
#include "enum_traits.hpp"
#include "path_util.hpp"
#include "string.hpp"
#include "ParserCommon.hpp"
#include "ConfigParser.hpp"

namespace Crowy
{
    static RHIPixelFormat toTextureFormat(std::string_view str){
        static std::unordered_map<std::string, RHIPixelFormat,
            StringHash, std::equal_to<>
        > text2TextureFormat = {
            // 8-bit formats
            {"R8_UNORM"         , RHIPixelFormat::R8_UNORM         },
            {"R8_SNORM"         , RHIPixelFormat::R8_SNORM         },
            {"R8_UINT"          , RHIPixelFormat::R8_UINT          },
            {"R8_SINT"          , RHIPixelFormat::R8_SINT          },
            // 16-bit formats
            {"R16_UNORM"        , RHIPixelFormat::R16_UNORM        },
            {"R16_SNORM"        , RHIPixelFormat::R16_SNORM        },
            {"R16_UINT"         , RHIPixelFormat::R16_UINT         },
            {"R16_SINT"         , RHIPixelFormat::R16_SINT         },
            {"R16_FLOAT"        , RHIPixelFormat::R16_FLOAT        },

            {"RG8_UNORM"        , RHIPixelFormat::RG8_UNORM        },
            {"RG8_SNORM"        , RHIPixelFormat::RG8_SNORM        },
            {"RG8_UINT"         , RHIPixelFormat::RG8_UINT         },
            {"RG8_SINT"         , RHIPixelFormat::RG8_SINT         },
            // 32-bit formats
            {"R32_UINT"         , RHIPixelFormat::R32_UINT         },
            {"R32_SINT"         , RHIPixelFormat::R32_SINT         },
            {"R32_FLOAT"        , RHIPixelFormat::R32_FLOAT        },

            {"RG16_UNORM"       , RHIPixelFormat::RG16_UNORM       },
            {"RG16_SNORM"       , RHIPixelFormat::RG16_SNORM       },
            {"RG16_UINT"        , RHIPixelFormat::RG16_UINT        },
            {"RG16_SINT"        , RHIPixelFormat::RG16_SINT        },
            {"RG16_FLOAT"       , RHIPixelFormat::RG16_FLOAT       },

            {"RGBA8_UNORM"      , RHIPixelFormat::RGBA8_UNORM      },
            {"RGBA8_UNORM_SRGB" , RHIPixelFormat::RGBA8_UNORM_SRGB },
            {"RGBA8_SNORM"      , RHIPixelFormat::RGBA8_SNORM      },
            {"RGBA8_UINT"       , RHIPixelFormat::RGBA8_UINT       },
            {"RGBA8_SINT"       , RHIPixelFormat::RGBA8_SINT       },

            {"BGRA8_UNORM"      , RHIPixelFormat::BGRA8_UNORM      },
            {"BGRA8_UNORM_SRGB" , RHIPixelFormat::BGRA8_UNORM_SRGB },
            // 64-bit formats
            {"RG32_UINT"        , RHIPixelFormat::RG32_UINT        },
            {"RG32_SINT"        , RHIPixelFormat::RG32_SINT        },
            {"RG32_FLOAT"       , RHIPixelFormat::RG32_FLOAT       },
            // 96-bit formats
            {"RGB32_FLOAT"      , RHIPixelFormat::RGB32_FLOAT      },

            {"RGBA16_UNORM"     , RHIPixelFormat::RGBA16_UNORM     },
            {"RGBA16_SNORM"     , RHIPixelFormat::RGBA16_SNORM     },
            {"RGBA16_UINT"      , RHIPixelFormat::RGBA16_UINT      },
            {"RGBA16_SINT"      , RHIPixelFormat::RGBA16_SINT      },
            {"RGBA16_FLOAT"     , RHIPixelFormat::RGBA16_FLOAT     },
            // 128-bit formats
            {"RGBA32_UINT"      , RHIPixelFormat::RGBA32_UINT      },
            {"RGBA32_SINT"      , RHIPixelFormat::RGBA32_SINT      },
            {"RGBA32_FLOAT"     , RHIPixelFormat::RGBA32_FLOAT     },
            // Depth/stencil formats
            {"D16_UNORM"        , RHIPixelFormat::D16_UNORM        },
            {"D24_UNORM_S8_UINT", RHIPixelFormat::D24_UNORM_S8_UINT},
            {"D32_FLOAT"        , RHIPixelFormat::D32_FLOAT        },
            {"D32_FLOAT_S8_UINT", RHIPixelFormat::D32_FLOAT_S8_UINT},
            // Compressed formats
            {"BC1_UNORM"        , RHIPixelFormat::BC1_UNORM        },
            {"BC1_UNORM_SRGB"   , RHIPixelFormat::BC1_UNORM_SRGB   },
            {"BC2_UNORM"        , RHIPixelFormat::BC2_UNORM        },
            {"BC2_UNORM_SRGB"   , RHIPixelFormat::BC2_UNORM_SRGB   },
            {"BC3_UNORM"        , RHIPixelFormat::BC3_UNORM        },
            {"BC3_UNORM_SRGB"   , RHIPixelFormat::BC3_UNORM_SRGB   },
            {"BC4_UNORM"        , RHIPixelFormat::BC4_UNORM        },
            {"BC4_SNORM"        , RHIPixelFormat::BC4_SNORM        },
            {"BC5_UNORM"        , RHIPixelFormat::BC5_UNORM        },
            {"BC5_SNORM"        , RHIPixelFormat::BC5_SNORM        },
            {"BC6H_UF16"        , RHIPixelFormat::BC6H_UF16        },
            {"BC6H_SF16"        , RHIPixelFormat::BC6H_SF16        },
            {"BC7_UNORM"        , RHIPixelFormat::BC7_UNORM        },
            {"BC7_UNORM_SRGB"   , RHIPixelFormat::BC7_UNORM_SRGB   },
        };
        auto upper = to_upper(str);

        auto it = text2TextureFormat.find(upper);
        if(it == text2TextureFormat.end()){
            return RHIPixelFormat::Unknown;
        }
        return it->second;
    }

    static RHIFilter toFilter(std::string_view str){
        static std::unordered_map<std::string, RHIFilter,
            StringHash, std::equal_to<>
        > text2Filter = {
            {"NEAREST", RHIFilter::Nearest},
            {"LINEAR" , RHIFilter::Linear}
        };

        auto upper = to_upper(str);

        auto it = text2Filter.find(upper);
        if(it == text2Filter.end()){
            // fallback (or throw error?)
            return RHIFilter::Nearest;
        }
        return it->second;
    }

    static RHIAddressMode toAddressMode(std::string_view str){
        static std::unordered_map<std::string, RHIAddressMode,
            StringHash, std::equal_to<>
        > text2AddressMode = {
            {"WRAP"  , RHIAddressMode::Wrap  },
            {"CLAMP" , RHIAddressMode::Clamp },
            {"MIRROR", RHIAddressMode::Mirror},
            {"BORDER", RHIAddressMode::Border}
        };

        auto upper = to_upper(str);

        auto it = text2AddressMode.find(upper);
        if(it == text2AddressMode.end()){
            // fallback (or throw error?)
            return RHIAddressMode::Wrap;
        }
        return it->second;
    }

    static std::unordered_map<std::string, RHISamplerState> compileSamplerPresets(
        const ParseResult& tempSamplers
    ){
        std::unordered_map<std::string, RHISamplerState> samplers;
        std::vector<BindError> errors;

        for(const auto& elm: tempSamplers.elements){
            const auto& node = tempSamplers.arena.nodes[elm.index];

            if(auto t = std::get_if<VTable>(&node)){
                auto name = readString(tempSamplers.arena, *t, errors, "name");
                if(!name.has_value())
                    continue;

                RHISamplerState desc{};

                auto filter = readString(tempSamplers.arena, *t, errors, "filter");
                auto address = readString(tempSamplers.arena, *t, errors, "address");

                if(filter.has_value() && address.has_value()){
                    desc.minFilter = desc.magFilter = desc.mipFilter = toFilter(*filter);
                    desc.addressU = desc.addressV = desc.addressW = toAddressMode(*address);
                }
                else{
                    auto minFilter = readString(tempSamplers.arena, *t, errors, "minFilter");
                    auto magFilter = readString(tempSamplers.arena, *t, errors, "magFilter");
                    auto mipFilter = readString(tempSamplers.arena, *t, errors, "mipFilter");
                    auto addressU = readString(tempSamplers.arena, *t, errors, "addressU");
                    auto addressV = readString(tempSamplers.arena, *t, errors, "addressV");
                    auto addressW = readString(tempSamplers.arena, *t, errors, "addressW");

                    // filter
                    if(minFilter.has_value())
                        desc.minFilter = toFilter(*minFilter);
                    else if(filter.has_value())
                        desc.minFilter = toFilter(*minFilter);
                    // cannot parse sampler preset
                    else continue;

                    if(magFilter.has_value())
                        desc.magFilter = toFilter(*magFilter);
                    else if(filter.has_value())
                        desc.magFilter = toFilter(*magFilter);
                    else continue;

                    if(mipFilter.has_value())
                        desc.mipFilter = toFilter(*mipFilter);
                    else if(filter.has_value())
                        desc.mipFilter = toFilter(*mipFilter);
                    else continue;

                    // address mode
                    if(addressU.has_value())
                        desc.addressU = toAddressMode(*addressU);
                    else if(filter.has_value())
                        desc.addressU = toAddressMode(*addressU);
                    else continue;

                    if(addressV.has_value())
                        desc.addressV = toAddressMode(*addressV);
                    else if(filter.has_value())
                        desc.addressV = toAddressMode(*addressV);
                    else continue;

                    if(addressW.has_value())
                        desc.addressW = toAddressMode(*addressW);
                    else if(filter.has_value())
                        desc.addressW = toAddressMode(*addressW);
                    else continue;
                }

                samplers.emplace(*name, desc);
            }
        }

        return samplers;
    }

    static std::unordered_map<std::string, RHITextureCreateDesc> compileTextures(
        const ParseResult& tempTextures
    ){
        std::unordered_map<std::string, RHITextureCreateDesc> out;
        std::vector<BindError> errors;

        for(const auto& elm: tempTextures.elements){
            const auto& node = tempTextures.arena.nodes[elm.index];

            if(auto t = std::get_if<VTable>(&node)){
                auto name   = readString(tempTextures.arena, *t, errors,   "name");
                auto width  = readFloat (tempTextures.arena, *t, errors,  "width", 0);
                auto height = readFloat (tempTextures.arena, *t, errors, "height", 0);
                auto fmt    = readString(tempTextures.arena, *t, errors, "format");

                if(!name || !fmt)
                    continue;

                out.emplace(*name, RHITextureCreateDesc{
                    .width  = static_cast<uint32_t>( width),
                    .height = static_cast<uint32_t>(height),
                    .format = toTextureFormat(*fmt)
                });
            }
        }
        reportError(errors);

        return out;
    }

    static std::optional<ShaderSpec> readShader(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n) return std::nullopt;

        auto src = std::get_if<VTable>(n);
        if(!src) return std::nullopt;

        auto vsFile = readString(arena, *src, errors, "vs_file");
        auto vsFunc = readString(arena, *src, errors, "vs_func", "vertex_main");
        auto fsFile = readString(arena, *src, errors, "fs_file");
        auto fsFunc = readString(arena, *src, errors, "fs_func", "fragment_main");

        if(!vsFile || !fsFile)
            return std::nullopt;

        return ShaderSpec{
            .vsFilePath = *vsFile,
            .vsFuncName = vsFunc,
            .fsFilePath = *fsFile,
            .fsFuncName = fsFunc,
        };
    }

    static std::vector<BindSpec> readBinds(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n) return {};

        auto arr = std::get_if<VArray>(n);
        if(!arr) return {};

        std::vector<BindSpec> v;
        v.resize(arr->elements.size());

        for(int i=0; i<arr->elements.size(); ++i){
            const VNode& elem = arena.nodes[arr->elements[i]];
            auto table = std::get_if<VTable>(&elem);
            if(!table){
                errors.push_back({
                    "element of Bind should be Table",
                    getLoc(elem)
                });
                break;
            }

            auto name = readString(arena, *table, errors, "name");
            auto slot = readString(arena, *table, errors, "slot");

            v[i] = BindSpec{
                .slot = *slot,
                .name = *name
            };
        }

        return v;
    }

    static std::vector<GraphicsPipelineBindSpec> readGraphicsPipelines(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key = "pipelines"
    ){
        const VNode* n = findField(arena, table, key);
        if(!n) return {};

        auto arr = std::get_if<VArray>(n);
        if(!arr) return {};

        std::vector<GraphicsPipelineBindSpec> v;
        v.resize(arr->elements.size());

        for(int i=0; i<arr->elements.size(); ++i){
            const VNode& elem = arena.nodes[arr->elements[i]];
            auto table = std::get_if<VTable>(&elem);
            if(!table){
                errors.push_back({
                    "element of Pipeline should be Table",
                    getLoc(elem)
                });
                break;
            }

        #ifdef CROWY_METALRHI
            auto shader = readShader(arena, *table, errors, "metal_shader");
        #elifdef CROWY_D3DRHI
            auto shader = readShader(arena, *table, errors, "d3d_shader");
        #endif
            if(!shader.has_value()){
                errors.push_back({
                    "element of Pipeline should have shader",
                    getLoc(elem)
                });
                break;
            }

            auto fs_textures = readBinds(arena, *table, errors, "fs_textures");
            auto fs_samplers = readBinds(arena, *table, errors, "fs_samplers");
            auto fs_cbuffers = readBinds(arena, *table, errors, "fs_cbuffers");

            // TODO. parse rasterizer state, depth stencil state, blend state

            auto renderType = readString(arena, *table, errors, "renderType");

            v[i].shader = *shader;
            v[i].fs = {
                .textures = std::move(fs_textures),
                .samplers = std::move(fs_samplers),
                .cbuffers = std::move(fs_cbuffers)
            };

            if(renderType.has_value())
                v[i].renderType = *renderType;
        }
        reportError(errors);

        return v;
    }

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
        auto upper = to_upper(str);

        auto it = text2FieldType.find(upper);
        if(it == text2FieldType.end()){
            return CBufferFieldType::Unknown;
        }
        return it->second;
    }

    void readCBufferData(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        CBuffer& cbuffer
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

    static std::unordered_map<std::string, CBuffer> compileCBuffers(
        const ParseResult& tempCBuffers
    ){
        std::unordered_map<std::string, CBuffer> cbuffers;
        std::vector<BindError> errors;

        for(size_t i=0; i<tempCBuffers.elements.size(); ++i){
            const auto& elm = tempCBuffers.elements[i];
            const auto& node = tempCBuffers.arena.nodes[elm.index];

            auto table = std::get_if<VTable>(&node);
            if(!table){
                errors.push_back({
                    "element of CBuffer should be Table",
                    getLoc(node)
                });
                break;
            }

            auto name = readString(tempCBuffers.arena, *table, errors, "name");
            if(!name)
                continue;

            CBuffer cbuffer;
            readCBufferData(tempCBuffers.arena, *table, errors, "value", cbuffer);

            cbuffers.emplace(*name, std::move(cbuffer));
        }
        reportError(errors);

        return cbuffers;
    }

    static std::vector<RenderPassSpec> compileRenderPasses(
        const ParseResult& tempPasses
    ){
        std::vector<RenderPassSpec> passes;
        std::vector<BindError> errors;

        // reserve pass slot and copy name.
        passes.resize(tempPasses.elements.size());
        for(size_t i=0; i<tempPasses.elements.size(); ++i){
            const auto& elm = tempPasses.elements[i];
            const auto& node = tempPasses.arena.nodes[elm.index];

            auto table = std::get_if<VTable>(&node);
            if(!table){
                // TODO. write Error
                break;
            }

            auto name = readString(tempPasses.arena, *table, errors, "name");
            if(!name){
                // TODO. write Error
                break;
            }

            passes[i].name = *name;

            auto outputs = readStringArray(tempPasses.arena, *table, errors, "outputs");
            auto depthOutput = readString(tempPasses.arena, *table, errors, "depthOutput");

            if(outputs.has_value())
                passes[i].outputs = *outputs;
            if(depthOutput.has_value())
                passes[i].depthOutput = *depthOutput;

            passes[i].pipelines = readGraphicsPipelines(tempPasses.arena, *table, errors, "pipelines");
        }
        reportError(errors);

        return passes;
    }

    static RenderSpec linkRender(
        std::unordered_map<std::string, RHITextureCreateDesc> textures,
        std::unordered_map<std::string, RHISamplerState> samplers,
        std::unordered_map<std::string, CBuffer> cbuffers,
        std::vector<RenderPassSpec> renderPasses
    ){
        for(auto& pass: renderPasses){
            // RenderTarget
            for(const auto& target: pass.outputs){
                if(auto it = textures.find(target); it != textures.end()){
                    auto& texDesc = it->second;
                    texDesc.usage = combine(texDesc.usage, RHITextureUsage::AllowRenderTarget);
                }
                else{
                    // TODO.ERROR! unresolved texture!
                }
            }

            for(auto& pipeline: pass.pipelines){
                // ShaderResource
                for(const auto& tex: pipeline.fs.textures){
                    if(auto it = textures.find(tex.name); it != textures.end()){
                        auto& texDesc = it->second;
                        texDesc.usage = combine(texDesc.usage, RHITextureUsage::AllowShaderRead);
                    }
                    else{
                        // TODO. ERROR! unresolved texture!
                    }
                }

                // DepthTarget
                if(!pass.depthOutput.empty()){
                    if(auto it = textures.find(pass.depthOutput); it != textures.end()){
                        auto& texDesc = it->second;
                        texDesc.usage = combine(texDesc.usage, RHITextureUsage::AllowDepthStencil);

                        pipeline.depthStencil = RHIDepthStencilState{
                            .format = texDesc.format,
                            .depthWriteEnable = true
                        };
                    }
                    else{
                        // TODO. ERROR! unresolved texture!
                    }
                }

                std::erase_if(
                    pipeline.fs.samplers,
                    [&samplers](const auto& bind){
                        return samplers.find(bind.name) == samplers.end();
                    }
                );

                std::erase_if(
                    pipeline.fs.cbuffers,
                    [&cbuffers](const auto& bind){
                        return cbuffers.find(bind.name) == cbuffers.end();
                    }
                );
            }
        }

        return RenderSpec{
            .textures = textures,
            .samplers = samplers,
            .cbuffers = cbuffers,
            .renderPasses = renderPasses
        };
    }

    RenderSpec parseRenderFromFile(const std::filesystem::path& renderFile){
        auto u8strPath = to_utf8String(renderFile);
        toml::parse_result pr = toml::parse_file(u8strPath);
        if(pr.empty())
            return {};

        auto tempTextures = parseFromTable(*pr.as_table(), "textures");
        auto textures = compileTextures(tempTextures);
        auto tempSamplers = parseFromTable(*pr.as_table(), "samplers");
        auto samplers = compileSamplerPresets(tempSamplers);
        auto tempCbuffers = parseFromTable(*pr.as_table(), "cbuffers");
        auto cbuffers = compileCBuffers(tempCbuffers);

        auto tempPasses = parseFromTable(*pr.as_table(), "passes");
        auto passes = compileRenderPasses(tempPasses);

        return linkRender(textures, samplers, cbuffers, passes);
    }

    RenderSpec parseRenderFromString(std::string_view renderText){
        toml::parse_result pr = toml::parse(renderText);
        if(pr.empty())
            return {};

        auto tempTextures = parseFromTable(*pr.as_table(), "textures");
        auto textures = compileTextures(tempTextures);
        auto tempSamplers = parseFromTable(*pr.as_table(), "samplers");
        auto samplers = compileSamplerPresets(tempSamplers);
        auto tempCBuffers = parseFromTable(*pr.as_table(), "cbuffers");
        auto cbuffers = compileCBuffers(tempCBuffers);

        auto tempPasses = parseFromTable(*pr.as_table(), "passes");
        auto passes = compileRenderPasses(tempPasses);

        return linkRender(textures, samplers, cbuffers, passes);
    }
}