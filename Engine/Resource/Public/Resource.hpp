#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include "math.hpp"
#include "RHIDefinitions.h"
#include "ResourceHandle.hpp"
#include "ResourceRequest.hpp"

namespace Crowy
{
    struct Submesh{
        RHIBufferHandle vertexBuffer;
        RHIBufferHandle indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;

        inline bool isValid() const{
            return vertexBuffer.isValid() &&
                   vertexCount > 0;
        }

        inline bool hasIndices() const{
            return indexBuffer.isValid() &&
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
    };

    struct Shader{
        using Request = ShaderRequest;

        RHIShaderHandle vertexShader;
        RHIShaderHandle fragmentShader;

        inline bool isValid() const{
            return vertexShader.isValid() &&
                   fragmentShader.isValid();
        }
    };

    void initResourceModule(class RHIDevice&);
    void deinitResourceModule();

    MeshHandle        getOrLoad(       MeshRequest);
    MaterialSetHandle getOrLoad(MaterialSetRequest);
    ShaderHandle      getOrLoad(     ShaderRequest);

    using        MeshView = std::span<const  Submesh>;
    using MaterialSetView = std::span<const Material>;

    MeshView        get(       MeshHandle);
    MaterialSetView get(MaterialSetHandle);
    Shader*         get(     ShaderHandle);
}