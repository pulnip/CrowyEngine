#include "enum_traits.hpp"
#include "path_util.hpp"
#include "string.hpp"
#include <toml++/toml.hpp>
#include "ParserCommon.hpp"
#include "RenderPassBinder.hpp"
#include "RenderParser.hpp"

namespace Crowy
{
    static RHITextureFormat toTextureFormat(std::string_view str){
        static std::unordered_map<std::string, RHITextureFormat,
            StringHash, std::equal_to<>
        > text2TextureFormat = {
            // 8-bit formats
            {"R8_UNORM"         , RHITextureFormat::R8_UNORM         },
            {"R8_SNORM"         , RHITextureFormat::R8_SNORM         },
            {"R8_UINT"          , RHITextureFormat::R8_UINT          },
            {"R8_SINT"          , RHITextureFormat::R8_SINT          },
            // 16-bit formats
            {"R16_UNORM"        , RHITextureFormat::R16_UNORM        },
            {"R16_SNORM"        , RHITextureFormat::R16_SNORM        },
            {"R16_UINT"         , RHITextureFormat::R16_UINT         },
            {"R16_SINT"         , RHITextureFormat::R16_SINT         },
            {"R16_FLOAT"        , RHITextureFormat::R16_FLOAT        },

            {"RG8_UNORM"        , RHITextureFormat::RG8_UNORM        },
            {"RG8_SNORM"        , RHITextureFormat::RG8_SNORM        },
            {"RG8_UINT"         , RHITextureFormat::RG8_UINT         },
            {"RG8_SINT"         , RHITextureFormat::RG8_SINT         },
            // 32-bit formats
            {"R32_UINT"         , RHITextureFormat::R32_UINT         },
            {"R32_SINT"         , RHITextureFormat::R32_SINT         },
            {"R32_FLOAT"        , RHITextureFormat::R32_FLOAT        },

            {"RG16_UNORM"       , RHITextureFormat::RG16_UNORM       },
            {"RG16_SNORM"       , RHITextureFormat::RG16_SNORM       },
            {"RG16_UINT"        , RHITextureFormat::RG16_UINT        },
            {"RG16_SINT"        , RHITextureFormat::RG16_SINT        },
            {"RG16_FLOAT"       , RHITextureFormat::RG16_FLOAT       },

            {"RGBA8_UNORM"      , RHITextureFormat::RGBA8_UNORM      },
            {"RGBA8_UNORM_SRGB" , RHITextureFormat::RGBA8_UNORM_SRGB },
            {"RGBA8_SNORM"      , RHITextureFormat::RGBA8_SNORM      },
            {"RGBA8_UINT"       , RHITextureFormat::RGBA8_UINT       },
            {"RGBA8_SINT"       , RHITextureFormat::RGBA8_SINT       },

            {"BGRA8_UNORM"      , RHITextureFormat::BGRA8_UNORM      },
            {"BGRA8_UNORM_SRGB" , RHITextureFormat::BGRA8_UNORM_SRGB },
            // 64-bit formats
            {"RG32_UINT"        , RHITextureFormat::RG32_UINT        },
            {"RG32_SINT"        , RHITextureFormat::RG32_SINT        },
            {"RG32_FLOAT"       , RHITextureFormat::RG32_FLOAT       },
            // 96-bit formats
            {"RGB32_FLOAT"      , RHITextureFormat::RGB32_FLOAT      },

            {"RGBA16_UNORM"     , RHITextureFormat::RGBA16_UNORM     },
            {"RGBA16_SNORM"     , RHITextureFormat::RGBA16_SNORM     },
            {"RGBA16_UINT"      , RHITextureFormat::RGBA16_UINT      },
            {"RGBA16_SINT"      , RHITextureFormat::RGBA16_SINT      },
            {"RGBA16_FLOAT"     , RHITextureFormat::RGBA16_FLOAT     },
            // 128-bit formats
            {"RGBA32_UINT"      , RHITextureFormat::RGBA32_UINT      },
            {"RGBA32_SINT"      , RHITextureFormat::RGBA32_SINT      },
            {"RGBA32_FLOAT"     , RHITextureFormat::RGBA32_FLOAT     },
            // Depth/stencil formats
            {"D16_UNORM"        , RHITextureFormat::D16_UNORM        },
            {"D24_UNORM_S8_UINT", RHITextureFormat::D24_UNORM_S8_UINT},
            {"D32_FLOAT"        , RHITextureFormat::D32_FLOAT        },
            {"D32_FLOAT_S8_UINT", RHITextureFormat::D32_FLOAT_S8_UINT},
            // Compressed formats
            {"BC1_UNORM"        , RHITextureFormat::BC1_UNORM        },
            {"BC1_UNORM_SRGB"   , RHITextureFormat::BC1_UNORM_SRGB   },
            {"BC2_UNORM"        , RHITextureFormat::BC2_UNORM        },
            {"BC2_UNORM_SRGB"   , RHITextureFormat::BC2_UNORM_SRGB   },
            {"BC3_UNORM"        , RHITextureFormat::BC3_UNORM        },
            {"BC3_UNORM_SRGB"   , RHITextureFormat::BC3_UNORM_SRGB   },
            {"BC4_UNORM"        , RHITextureFormat::BC4_UNORM        },
            {"BC4_SNORM"        , RHITextureFormat::BC4_SNORM        },
            {"BC5_UNORM"        , RHITextureFormat::BC5_UNORM        },
            {"BC5_SNORM"        , RHITextureFormat::BC5_SNORM        },
            {"BC6H_UF16"        , RHITextureFormat::BC6H_UF16        },
            {"BC6H_SF16"        , RHITextureFormat::BC6H_SF16        },
            {"BC7_UNORM"        , RHITextureFormat::BC7_UNORM        },
            {"BC7_UNORM_SRGB"   , RHITextureFormat::BC7_UNORM_SRGB   },
        };
        auto upper = toUpper(str);

        auto it = text2TextureFormat.find(upper);
        if(it == text2TextureFormat.end()){
            return RHITextureFormat::Unknown;
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

        auto upper = toUpper(str);

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

        auto upper = toUpper(str);

        auto it = text2AddressMode.find(upper);
        if(it == text2AddressMode.end()){
            // fallback (or throw error?)
            return RHIAddressMode::Wrap;
        }
        return it->second;
    }

    static SamplerPresets compileSamplerPresets(
        const ParseResult& tempSPs
    ){
        SamplerPresets out;
        std::vector<BindError> errors;

        for(const auto& elm: tempSPs.elements){
            const auto& node = tempSPs.arena.nodes[elm.index];

            if(auto t = std::get_if<VTable>(&node)){
                auto name = readString(tempSPs.arena, *t, errors, "name");
                if(!name.has_value())
                    continue;

                RHISamplerState desc{};

                auto filter = readString(tempSPs.arena, *t, errors, "filter");
                auto address = readString(tempSPs.arena, *t, errors, "address");

                if(filter.has_value() && address.has_value()){
                    desc.minFilter = desc.magFilter = desc.mipFilter = toFilter(*filter);
                    desc.addressU = desc.addressV = desc.addressW = toAddressMode(*address);
                }
                else{
                    auto minFilter = readString(tempSPs.arena, *t, errors, "minFilter");
                    auto magFilter = readString(tempSPs.arena, *t, errors, "magFilter");
                    auto mipFilter = readString(tempSPs.arena, *t, errors, "mipFilter");
                    auto addressU = readString(tempSPs.arena, *t, errors, "addressU");
                    auto addressV = readString(tempSPs.arena, *t, errors, "addressV");
                    auto addressW = readString(tempSPs.arena, *t, errors, "addressW");

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

                out.emplace(*name, desc);
            }
        }

        return out;
    }

    static std::unordered_map<std::string, RHITextureCreateDesc> compileRenderTarget(
        const ParseResult& tempRTs
    ){
        std::unordered_map<std::string, RHITextureCreateDesc> out;
        std::vector<BindError> errors;

        for(const auto& elm: tempRTs.elements){
            const auto& node = tempRTs.arena.nodes[elm.index];

            if(auto t = std::get_if<VTable>(&node)){
                auto name   = readString(tempRTs.arena, *t, errors,   "name");
                auto width  = readFloat (tempRTs.arena, *t, errors,  "width", 0);
                auto height = readFloat (tempRTs.arena, *t, errors, "height", 0);
                auto fmt    = readString(tempRTs.arena, *t, errors, "format");

                if(!name || !fmt)
                    continue;

                out.emplace(*name, RHITextureCreateDesc{
                    .width  = static_cast<uint32_t>( width),
                    .height = static_cast<uint32_t>(height),
                    .format = toTextureFormat(*fmt),
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = *name
                #endif
                });
            }
        }

        return out;
    }

    static std::vector<RenderPassSpec> compileRender(
        const ParseResult& tempPasses,
        const RenderPassBinderRegistry& registry
    ){
        std::vector<RenderPassSpec> out;
        RenderPassElementBindPlan plan;

        // reserve pass slot and copy name.
        out.resize(tempPasses.elements.size());
        for(size_t i=0; i<tempPasses.elements.size(); ++i){
            const auto& elm = tempPasses.elements[i];
            const auto& node = tempPasses.arena.nodes[elm.index];

            if(auto table = std::get_if<VTable>(&node)){
                auto name = readString(tempPasses.arena, *table, plan.errors, "name");
                auto inputs = readStringArray(tempPasses.arena, *table, plan.errors, "inputs");
                auto targets = readStringArray(tempPasses.arena, *table, plan.errors, "targets");
                auto depthTarget = readString(tempPasses.arena, *table, plan.errors, "depthTarget");

                auto renderType = readString(tempPasses.arena, *table, plan.errors, "renderType");

                // TODO. parse rasterizer state, depth stencil state, blend state

                if(name.has_value())
                    out[i].name = *name;

                if(inputs.has_value())
                    out[i].inputs = *inputs;
                if(targets.has_value())
                    out[i].targets = *targets;
                if(depthTarget.has_value())
                    out[i].depthTarget = *depthTarget;

                if(renderType.has_value())
                    out[i].renderType = *renderType;
            }
            else{
                // TODO. write Error
            }                                                                  
        }

        bindAndErrorReport(tempPasses, registry, plan);
        reportError(plan.errors);

        // Freeze(Create SoA + connect index)
        SamplerBinder::freeze(out, plan);
        ShaderBinder::freeze(out, plan);

        return out;
    }

    static RenderSpec linkRender(
        std::unordered_map<std::string, RHITextureCreateDesc> renderTargets,
        std::vector<RenderPassSpec> passes
    ){
        for(auto& pass: passes){
            // ShaderResource
            for(const auto& input: pass.inputs){
                if(auto it = renderTargets.find(input); it != renderTargets.end()){
                    auto& texDesc = it->second;
                    texDesc.usage = combine(texDesc.usage, RHITextureUsage::ShaderResource);
                }
                else{
                    // TODO. ERROR! unresolved texture!
                }
            }

            // RenderTarget
            for(const auto& target: pass.targets){
                if(auto it = renderTargets.find(target); it != renderTargets.end()){
                    auto& texDesc = it->second;
                    texDesc.usage = combine(texDesc.usage, RHITextureUsage::RenderTarget);
                }
                else{
                    // TODO.ERROR! unresolved texture!
                }
            }

            // DepthTarget
            if(!pass.depthTarget.empty()){
                if(auto it = renderTargets.find(pass.depthTarget); it != renderTargets.end()){
                    auto& texDesc = it->second;
                    texDesc.usage = combine(texDesc.usage, RHITextureUsage::DepthStencil);

                    pass.depthStencil = RHIDepthStencilState{
                        .format = texDesc.format,
                        .depthWriteEnable = true
                    };
                }
                else{
                    // TODO. ERROR! unresolved texture!
                }
            }
        }

        // TODO

        return RenderSpec{
            .renderTargets = renderTargets,
            .passes = passes
        };
    }

    RenderSpec parseRenderFromFile(const std::filesystem::path& renderFile){
        auto u8strPath = to_utf8String(renderFile);
        toml::parse_result pr = toml::parse_file(u8strPath);
        if(pr.empty())
            return {};

        auto tempSamplerPresets = parseFromTable(*pr.as_table(), "sampler_presets");
        auto samplerPresets = compileSamplerPresets(tempSamplerPresets);
        auto binderRegistry = makeRenderPassBinderRegistry(samplerPresets);

        auto tempRenderTargets = parseFromTable(*pr.as_table(), "render_targets");
        auto renderTargets = compileRenderTarget(tempRenderTargets);

        auto tempPasses = parseFromTable(*pr.as_table(), "passes");
        auto passes = compileRender(tempPasses, binderRegistry);

        return linkRender(renderTargets, passes);
    }

    RenderSpec parseRenderFromString(std::string_view renderText){
        toml::parse_result pr = toml::parse(renderText);
        if(pr.empty())
            return {};

        auto tempSamplerPresets = parseFromTable(*pr.as_table(), "sampler_presets");
        auto samplerPresets = compileSamplerPresets(tempSamplerPresets);
        auto binderRegistry = makeRenderPassBinderRegistry(samplerPresets);

        auto tempRenderTargets = parseFromTable(*pr.as_table(), "render_targets");
        auto renderTargets = compileRenderTarget(tempRenderTargets);

        auto tempPasses = parseFromTable(*pr.as_table(), "passes");
        auto passes = compileRender(tempPasses, binderRegistry);

        return linkRender(renderTargets, passes);
    }
}