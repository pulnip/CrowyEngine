#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "math.hpp"
#include "RHIDefinitions.h"

namespace Crowy
{
    // Standard vertex format
    // Matches common 3D model formats (glTF, FBX, OBJ)
    struct Vertex{
        Vec3 position;
        Vec3 normal;
        Vec2 texCoord;
        Vec4 tangent;  // xyz = tangent direction, w = handedness sign

        // Optional: vertex colors, bone weights, etc. can be added later
    };
    static_assert(sizeof(Vertex) == 48, "Vertex should be 48 bytes");
    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex must be trivially copyable");

    // Different 3D tools use different conventions
    struct AxisInfo{
        bool leftHanded = true;
        char upAxis = 'Y';
        char forwardAxis = 'Z';
        bool flipTexCoordV = true; // Flip V coordinate (OpenGL vs DirectX)
        double unitScale = 0.01f;  // Conversion factor (e.g., 0.01 for cm to meters)
    };

    // Axis-Aligned Bounding Box
    struct AABB{
        Vec3 min = zeros();
        Vec3 max = zeros();

        inline Vec3 center() const{
            return (min + max)/2;
        }
        inline Vec3 extents() const{
            return (max - min)/2;
        }
        inline bool isValid() const {
            return min.x <= max.x &&
                   min.y <= max.y &&
                   min.z <= max.z;
        }
    };

    // for shading model
    enum class MaterialType: uint8_t{
        Unlit = 0, // No lighting, just texture/color
        PBR   = 1, // Physically-Based Rendering
    };

    enum TextureFlags: uint16_t{
        TEX_None         = 0,
        TEX_SRGB         = 1 << 0, // Texture is in sRGB color space
        TEX_GenerateMips = 1 << 1  // Generate mipmaps
    };

    // Texture reference in a material
    struct TextureRef{
        std::string path;  // Path to texture file
        // TextureUsage usage;
        uint16_t flags = TEX_None;
    };

    // Texture usage semantic
    enum class TextureSemantic: uint8_t{
        BaseColor         = 0, // Albedo / Diffuse
        Normal            = 1, // Normal map (tangent space)
        MetallicRoughness = 2, // R=unused, G=Roughness, B=Metallic (glTF 2.0 convention)
        Emissive          = 3, // Emission map
        Occlusion         = 4, // Ambient occlusion

        // Future: add more as needed
        // Height, Opacity, etc.
    };

    // Describes how a surface should be shaded
    struct MaterialRef{
        std::string name;
        MaterialType type = MaterialType::PBR;

        // Textures keyed by usage
        std::unordered_map<TextureSemantic, TextureRef> textures;

        // Material parameters (when textures are not present)
        Vec4 baseColor = Vec4{1.0f, 1.0f, 1.0f, 1.0f};
        float metallic = 0.0f;
        float roughness = 1.0f;
        Vec3 emissive = zeros();

        // Check if material has a specific texture
        inline bool hasTexture(TextureSemantic semantic) const{
            return textures.find(semantic) != textures.end();
        }
    };

    // mesh part with single material
    struct SubmeshData{
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        RHIPrimitiveTopology primitiveType = RHIPrimitiveTopology::TriangleList;

        // Which material to use
        std::string materialSlotName;

        inline uint32_t vertexCount() const{
            return static_cast<uint32_t>(vertices.size());
        }
        inline uint32_t indexCount() const{
            return static_cast<uint32_t>(indices.size());
        }
        inline uint32_t triangleCount() const{
            return primitiveType == RHIPrimitiveTopology::TriangleList ?
                indexCount()/3 : 0;
        }
    };

    // Runtime mesh representation
    // A mesh can contain multiple submeshes with different materials
    struct ModelData{
        // Metadata
        AxisInfo axisInfo;
        AABB bounds;

        // Geometry
        std::vector<SubmeshData> submeshes;

        // Materials (keyed by slot name)
        std::unordered_map<std::string, MaterialRef> materials;

        inline uint32_t submeshCount() const{
            return static_cast<uint32_t>(submeshes.size());
        }
        inline uint32_t materialCount() const{
            return static_cast<uint32_t>(materials.size());
        }

        inline uint32_t totalVertexCount() const{
            uint32_t count = 0;
            for(const auto& submesh : submeshes){
                count += submesh.vertexCount();
            }
            return count;
        }

        inline uint32_t totalIndexCount() const{
            uint32_t count = 0;
            for(const auto& submesh: submeshes){
                count += submesh.indexCount();
            }
            return count;
        }

        inline bool isValid() const{
            return !submeshes.empty() && bounds.isValid();
        }

        // Find material by slot name
        inline const MaterialRef* findMaterial(const std::string& slotName) const{
            auto it = materials.find(slotName);
            return it != materials.end() ?
                &it->second : nullptr;
        }
    };
}
