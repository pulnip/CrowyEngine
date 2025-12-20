#pragma once

#include <string>
#include <vector>
#include "generic_handle.hpp"
#include "math.hpp"
#include "RHIDefinitions.h"

namespace Crowy
{
    template<typename T>
    struct ResourceTraits;

    struct Submesh{
        RHIBufferHandle vertexBuffer;
        RHIBufferHandle indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;

        inline bool isValid() const{
            return vertexBuffer.isValid() && vertexCount > 0;
        }

        inline bool hasIndices() const{
            return indexBuffer.isValid() && indexCount > 0;
        }
    };
    using SubmeshHandle = generic_handle<Submesh>;

    struct Mesh{
        std::vector<SubmeshHandle> submeshes;

        inline size_t submeshCount() const{ return submeshes.size(); }
        inline bool empty() const{ return submeshes.empty(); }
        inline bool isValid() const{ return !submeshes.empty(); }
    };
    using MeshHandle = generic_handle<Mesh>;

        struct MeshRequest{
        std::string meshKey;                        // Key for cache lookup
        std::vector<SubmeshHandle> submeshHandles;  // Pre-loaded submesh handles
    };

    struct MeshKey{
        std::string meshKey;

        auto operator<=>(const MeshKey&) const = default;
    };

    struct MeshKeyHash{
        inline size_t operator()(const MeshKey& k) const noexcept{
            return std::hash<std::string>{}(k.meshKey);
        }
    };

    template<>
    struct ResourceTraits<Mesh>{
        using Request = MeshRequest;
        using Key     = MeshKey;
        using KeyHash = MeshKeyHash;

        inline static Key makeKey(const Request& request){
            return Key{
                .meshKey = request.meshKey
            };
        }

        inline static Mesh load(const Request& request){
            return Mesh{
                .submeshes = request.submeshHandles
            };
        }
    };

    enum class MaterialType: uint8_t{
        Unlit = 0,  // No lighting, just texture/color
        PBR = 1     // Physically-Based Rendering
    };

    enum class TextureUsage: uint8_t{
        BaseColor = 0,  // Albedo / Diffuse
        Normal = 1,     // Normal map (tangent space)
        MetallicRoughness = 2,  // R=unused, G=Roughness, B=Metallic (glTF 2.0 convention)
        Emissive = 3,   // Emission map
        Occlusion = 4,  // Ambient occlusion
    };

    enum TextureFlags: uint16_t{
        TEX_None = 0,
        TEX_SRGB = 1 << 0,  // Texture is in sRGB color space
        TEX_GenerateMips = 1 << 1  // Generate mipmaps
    };

    struct TextureDescriptor{
        std::string uri;  // Path to texture file
        TextureUsage usage;
        uint16_t flags = TEX_None;
    };

    struct MaterialDescriptor{
        std::string name;
        MaterialType type = MaterialType::PBR;

        // Textures keyed by usage
        std::unordered_map<TextureUsage, TextureDescriptor> textures;

        // Material parameters (when textures are not present)
        Vec4 baseColorFactor = Vec4{1.0f, 1.0f, 1.0f, 1.0f};
        double metallicFactor = 0.0f;
        double roughnessFactor = 1.0f;
        Vec3 emissiveFactor = zeros();

        // Check if material has a specific texture
        inline bool hasTexture(TextureUsage usage) const{
            return textures.find(usage) != textures.end();
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

        inline bool hasAlbedoMap() const{ return albedoMap.isValid(); }
        inline bool hasNormalMap() const{ return normalMap.isValid(); }
        inline bool hasMetallicRoughnessMap() const{ return metallicRoughnessMap.isValid(); }
        inline bool hasEmissiveMap() const{ return emissiveMap.isValid(); }
    };
    using MaterialHandle = generic_handle<Material>;

    struct MaterialRequest{
        std::string meshKey;                        // Key for cache lookup
        uint32_t materialIndex = 0;                 // Which material in the mesh
        const MaterialDescriptor* data = nullptr;   // CPU material data
        // Texture handles (already loaded by ResourceLoader)
        RHITextureHandle albedoMap;
        RHITextureHandle normalMap;
        RHITextureHandle metallicRoughnessMap;
        RHITextureHandle emissiveMap;
    };

    struct MaterialKey{
        std::string meshKey;
        uint32_t materialIndex;

        auto operator<=>(const MaterialKey&) const = default;
    };

    struct MaterialKeyHash{
        inline size_t operator()(const MaterialKey& k) const noexcept{
            size_t h1 = std::hash<std::string>{}(k.meshKey);
            size_t h2 = std::hash<uint32_t>{}(k.materialIndex);
            return h1 ^ (h2 << 1);
        }
    };

    template<>
    struct ResourceTraits<Material>{
        using Request = MaterialRequest;
        using Key     = MaterialKey;
        using KeyHash = MaterialKeyHash;

        inline static Key makeKey(const Request& request){
            return Key{
                .meshKey = request.meshKey,
                .materialIndex = request.materialIndex
            };
        }

        inline static Material load(const Request& request){
            Material material{};

            if(request.data){
                const auto& desc = *request.data;
                material.albedo = desc.baseColorFactor;
                material.metallic = static_cast<float>(desc.metallicFactor);
                material.roughness = static_cast<float>(desc.roughnessFactor);
                material.alpha = desc.baseColorFactor.w;
            }

            // Assign pre-loaded texture handles
            material.albedoMap = request.albedoMap;
            material.normalMap = request.normalMap;
            material.metallicRoughnessMap = request.metallicRoughnessMap;
            material.emissiveMap = request.emissiveMap;

            return material;
        }
    };

    struct MaterialSet{
        std::vector<MaterialHandle> materials;

        inline size_t materialCount() const{ return materials.size(); }
        inline bool empty() const{ return materials.empty(); }
        inline bool isValid() const{ return !materials.empty(); }
    };
    using MaterialSetHandle = generic_handle<MaterialSet>;


    struct MaterialSetRequest{
        std::string meshKey;                            // Key for cache lookup
        std::vector<MaterialHandle> materialHandles;    // Pre-loaded material handles
    };

    struct MaterialSetKey{
        std::string meshKey;

        auto operator<=>(const MaterialSetKey&) const = default;
    };

    struct MaterialSetKeyHash{
        inline size_t operator()(const MaterialSetKey& k) const noexcept{
            return std::hash<std::string>{}(k.meshKey);
        }
    };

    template<>
    struct ResourceTraits<MaterialSet>{
        using Request = MaterialSetRequest;
        using Key     = MaterialSetKey;
        using KeyHash = MaterialSetKeyHash;

        inline static Key makeKey(const Request& request){
            return Key{
                .meshKey = request.meshKey
            };
        }

        inline static MaterialSet load(const Request& request){
            return MaterialSet{
                .materials = request.materialHandles
            };
        }
    };

    struct Shader{
        RHIShaderHandle vertexShader;
        RHIShaderHandle fragmentShader;

        inline bool isValid() const{
            return vertexShader.isValid() && fragmentShader.isValid();
        }
    };
    using ShaderHandle = generic_handle<Shader>;
}