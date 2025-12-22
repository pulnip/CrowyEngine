#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "generic_handle.hpp"
#include "math.hpp"
#include "RHIDefinitions.h"

namespace Crowy
{
    struct LoadContext;

    struct Material{
        // PBR parameters
        Vec4 albedo = Vec4{1.0f, 1.0f, 1.0f, 1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        float alpha = 1.0f;

        // Texture maps (optional)
        RHITextureHandle albedoMap;
        RHITextureHandle normalMap;
        RHITextureHandle metallicRoughnessMap;
        RHITextureHandle emissiveMap;

        inline bool hasAlbedoMap() const{
            return albedoMap.isValid();
        }
        inline bool hasNormalMap() const{
            return normalMap.isValid();
        }
        inline bool hasMetallicRoughnessMap() const{
            return metallicRoughnessMap.isValid();
        }
        inline bool hasEmissiveMap() const{
            return emissiveMap.isValid();
        }

        struct Request{
            struct Key{
                std::string meshKey;
                uint32_t materialIndex;

                auto operator<=>(const Key&) const = default;
            };
            struct KeyHash{
                inline size_t operator()(const Key& k) const noexcept{
                    auto h1 = std::hash<std::string>{}(k.meshKey);
                    auto h2 = std::hash<uint32_t>{}(k.materialIndex);
                    return h1 ^ (h2 << 1);
                }
            };

            std::string meshKey;
            // Which material in the mesh
            uint32_t materialIndex;

            RHITextureHandle albedoMap;
            RHITextureHandle normalMap;
            RHITextureHandle metallicRoughnessMap;
            RHITextureHandle emissiveMap;

            inline Key key() const{
                return {meshKey, materialIndex};
            }
        };

        static Material make(const Request&, LoadContext&);
    };
    using MaterialHandle = generic_handle<Material>;

    class MaterialManager{
    public:
        MaterialManager();
        ~MaterialManager();

        MaterialHandle getOrLoad(const Material::Request&,
            LoadContext& ctx);
        Material*       get(MaterialHandle);
        const Material* get(MaterialHandle) const;
        void unload(MaterialHandle);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}