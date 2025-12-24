#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include "math.hpp"
#include "RHIDefinitions.h"
#include "RHIFWD.hpp"
#include "ResourceHandle.hpp"
#include "ResourceRequest.hpp"

namespace Crowy
{
    struct Submesh{
        RHIBufferPtr vertexBuffer;
        RHIBufferPtr indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;

        inline bool isValid() const{
            return vertexBuffer != nullptr &&
                   vertexCount > 0;
        }

        inline bool hasIndices() const{
            return indexBuffer != nullptr &&
                   indexCount > 0;
        }
    };

    struct Material{
        // PBR parameters
        Vec4 albedo = Vec4{1.0f, 1.0f, 1.0f, 1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        float alpha = 1.0f;

        // Texture maps (optional)
        RHITexturePtr albedoMap;
        RHITexturePtr normalMap;
        RHITexturePtr metallicRoughnessMap;
        RHITexturePtr emissiveMap;

        inline bool hasAlbedoMap() const{
            return albedoMap != nullptr;
        }
        inline bool hasNormalMap() const{
            return normalMap != nullptr;
        }
        inline bool hasMetallicRoughnessMap() const{
            return metallicRoughnessMap != nullptr;
        }
        inline bool hasEmissiveMap() const{
            return emissiveMap != nullptr;
        }
    };

    struct Shader{
        using Request = ShaderRequest;

        RHIShaderPtr vertexShader;
        RHIShaderPtr fragmentShader;

        inline bool isValid() const{
            return vertexShader   != nullptr &&
                   fragmentShader != nullptr;
        }
    };

    void initResourceModule(RHIDevice&);
    void deinitResourceModule();

    std::pair<MeshHandle, MaterialSetHandle> getOrLoad(ModelRequest);
    ShaderHandle                             getOrLoad(ShaderRequest);

    using        MeshView = std::span<const  Submesh>;
    using MaterialSetView = std::span<const Material>;

    MeshView        get(       MeshHandle);
    MaterialSetView get(MaterialSetHandle);
    Shader*         get(     ShaderHandle);
}