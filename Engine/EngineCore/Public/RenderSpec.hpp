#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "string.hpp"
#include "RenderDefinitions.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct ShaderSpec{
        std::filesystem::path vsFilePath;
        std::string vsFuncName;
        std::filesystem::path fsFilePath;
        std::string fsFuncName;
    };

    struct CBufferSpec{
        using FieldName = CBufferFieldName;
        using FieldType = CBufferFieldType;
        using FieldOffset = CBufferFieldOffset;
        using FieldMeta = CBufferFieldMeta;

        std::string name;
        uint32_t slot;
        std::unordered_map<
            CBufferFieldName, CBufferFieldMeta,
            StringHash, std::equal_to<>
        > meta;
        std::vector<std::byte> payload;

        struct ConstFieldProxy{
            const FieldType type;
            const std::byte& ref;

            template<typename T>
            operator T() const{
                T t;
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&t, &ref, size_of(type));

                return t;
            }
        };

        struct FieldProxy{
            const FieldType type;
            std::byte& ref;

            template<typename T>
            FieldProxy& operator=(const T& t){
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&ref, &t, size_of(type));
                return *this;
            }

            template<typename T>
            operator T() const{
                T t;
                if(is_convertible_to<T>(type) && sizeof(T) == size_of(type))
                    std::memcpy(&t, &ref, size_of(type));

                return t;
            }
        };

        inline auto newField(
            std::string_view name,
            FieldType type
        ){
            auto [it, succeed] = meta.try_emplace(std::string(name), FieldMeta{
                .type = type,
                .offset = payload.size()
            });

            if(succeed)
                payload.resize(payload.size() + size_of(type));

            auto& m = it->second;
            return FieldProxy{
                .type = m.type,
                .ref = payload[m.offset]
            };
        }

        std::optional<ConstFieldProxy> at(std::string_view name) const{
            auto it = meta.find(name);
            if(it == meta.end())
                return std::nullopt;

            auto& m = it->second;
            return ConstFieldProxy{
                .type = m.type,
                .ref = payload[m.offset]
            };
        }
        std::optional<FieldProxy> at(std::string_view name){
            auto it = meta.find(name);
            if(it == meta.end())
                return std::nullopt;

            auto& m = it->second;
            return FieldProxy{
                .type = m.type,
                .ref = payload[m.offset]
            };
        }

        inline const void* data() const noexcept{ return payload.data(); }
        inline auto size() const noexcept{ return payload.size(); }
    };

    struct RenderPassSpec{
        std::string name;

        // input Texture
        std::vector<std::string> inputs;
        // output RenderTarget
        std::vector<std::string> targets;
        // depth buffer
        std::string depthTarget;
        std::vector<RHISamplerState> fs_samplers;

        ShaderSpec shader;
        RenderType renderType;
        RHIRasterizerState rasterizer = {};
        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        RHIBlendState blend = {};

        std::vector<CBufferSpec> fs_cbuffers;
    };

    struct RenderSpec{
        std::unordered_map<std::string, RHITextureCreateDesc> renderTargets;
        std::vector<RenderPassSpec> passes;
    };
}