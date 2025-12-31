#include <cmath>
#include <numbers>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Log.hpp"
#include "ModelImporter.hpp"
#include "PathUtils.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    enum class SchemeKind{
        File     = 0,
        Embedded = 1,
        Unknown  = 2
    };

    // "scheme:path" -> {scheme, path}
    static std::pair<SchemeKind, std::string> splitSchemeAndPath(const std::string& uri){
        auto colon_pos = uri.find(':');
        if(colon_pos == std::string::npos)
            return {SchemeKind::Unknown, ""};

        auto schemeStr = uri.substr(0, colon_pos);
        auto pathStr = uri.substr(colon_pos + 1);

        if(schemeStr == "file")
            return {SchemeKind::File, pathStr};
        else if(schemeStr == "embedded")
            return {SchemeKind::Embedded, pathStr};
        else
            return {SchemeKind::Unknown, pathStr};
    }

    static ModelData createEmbeddedCube(){
        ModelData model;

        // Create single submesh
        SubmeshData submesh;
        submesh.primitiveType = RHIPrimitiveTopology::TriangleList;
        submesh.materialSlotName = "default";

        // Cube vertices (24 vertices, 4 per face for proper normals/UVs)
        submesh.vertices = {
            // Front face (Z-)
            Vertex{Vec3{-0.5f, -0.5f, -0.5f}, Vec3{0.0f, 0.0f, -1.0f}, Vec2{0.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, -0.5f, -0.5f}, Vec3{0.0f, 0.0f, -1.0f}, Vec2{1.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f,  0.5f, -0.5f}, Vec3{0.0f, 0.0f, -1.0f}, Vec2{1.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{-0.5f,  0.5f, -0.5f}, Vec3{0.0f, 0.0f, -1.0f}, Vec2{0.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},

            // Back face (Z+)
            Vertex{Vec3{-0.5f, -0.5f,  0.5f}, Vec3{0.0f, 0.0f, 1.0f}, Vec2{0.0f, 1.0f}, Vec4{-1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, -0.5f,  0.5f}, Vec3{0.0f, 0.0f, 1.0f}, Vec2{1.0f, 1.0f}, Vec4{-1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f,  0.5f,  0.5f}, Vec3{0.0f, 0.0f, 1.0f}, Vec2{1.0f, 0.0f}, Vec4{-1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{-0.5f,  0.5f,  0.5f}, Vec3{0.0f, 0.0f, 1.0f}, Vec2{0.0f, 0.0f}, Vec4{-1.0f, 0.0f, 0.0f, 1.0f}},

            // Left face (X-)
            Vertex{Vec3{-0.5f,  0.5f, -0.5f}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{0.0f, 1.0f}, Vec4{0.0f, 0.0f, -1.0f, 1.0f}},
            Vertex{Vec3{-0.5f, -0.5f, -0.5f}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{1.0f, 1.0f}, Vec4{0.0f, 0.0f, -1.0f, 1.0f}},
            Vertex{Vec3{-0.5f, -0.5f,  0.5f}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec4{0.0f, 0.0f, -1.0f, 1.0f}},
            Vertex{Vec3{-0.5f,  0.5f,  0.5f}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{0.0f, 0.0f}, Vec4{0.0f, 0.0f, -1.0f, 1.0f}},

            // Right face (X+)
            Vertex{Vec3{ 0.5f,  0.5f, -0.5f}, Vec3{1.0f, 0.0f, 0.0f}, Vec2{0.0f, 1.0f}, Vec4{0.0f, 0.0f, 1.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, -0.5f, -0.5f}, Vec3{1.0f, 0.0f, 0.0f}, Vec2{1.0f, 1.0f}, Vec4{0.0f, 0.0f, 1.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, -0.5f,  0.5f}, Vec3{1.0f, 0.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec4{0.0f, 0.0f, 1.0f, 1.0f}},
            Vertex{Vec3{ 0.5f,  0.5f,  0.5f}, Vec3{1.0f, 0.0f, 0.0f}, Vec2{0.0f, 0.0f}, Vec4{0.0f, 0.0f, 1.0f, 1.0f}},

            // Bottom face (Y-)
            Vertex{Vec3{-0.5f, -0.5f, -0.5f}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{0.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, -0.5f, -0.5f}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{1.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, -0.5f,  0.5f}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{-0.5f, -0.5f,  0.5f}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{0.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},

            // Top face (Y+)
            Vertex{Vec3{-0.5f,  0.5f, -0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{0.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f,  0.5f, -0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{1.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f,  0.5f,  0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{-0.5f,  0.5f,  0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{0.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
        };

        // Cube indices (12 triangles, 2 per face)
        submesh.indices = {
            // Front
             2,  0,  3,  1,  0,  2,
            // Back
             4,  5,  6,  6,  7,  4,
            // Left
            11,  8,  9,  9, 10, 11,
            // Right
            13, 12, 14, 15, 14, 12,
            // Bottom
            16, 17, 18, 18, 19, 16,
            // Top
            21, 20, 22, 22, 20, 23
        };

        model.submeshes.push_back(submesh);

        // Compute AABB
        model.bounds.min = Vec3{-0.5f, -0.5f, -0.5f};
        model.bounds.max = Vec3{ 0.5f,  0.5f,  0.5f};

        return model;
    }

    static ModelData createEmbeddedSphere(
        float radius=1.0f, int slices=32, int stacks=16
    ){
        ModelData model;

        SubmeshData submesh;
        submesh.primitiveType = RHIPrimitiveTopology::TriangleList;
        submesh.materialSlotName = "default";

        const float dTheta = 2.0f * std::numbers::pi_v<float> / slices;
        const float dPhi = std::numbers::pi_v<float> / stacks;

        // Generate vertices
        for(int i = 0; i <= stacks; ++i){
            const float phi = dPhi * i;
            const float y = radius * std::cos(phi);
            const float rad = radius * std::sin(phi);
            const float v = static_cast<float>(i) / stacks;

            for(int j = 0; j <= slices; ++j){
                const float theta = dTheta * j;
                const float x = rad * std::cos(theta);
                const float z = rad * std::sin(theta);
                const float u = static_cast<float>(j) / slices;

                // Normal is same as position for unit sphere
                Vec3 pos{x, y, z};
                Vec3 normal = pos;

                // Simple tangent calculation
                Vec3 tangent{-std::sin(theta), 0.0f, std::cos(theta)};

                submesh.vertices.push_back(Vertex{
                    pos,
                    normal,
                    Vec2{u, v},
                    Vec4{tangent.x, tangent.y, tangent.z, 1.0f}
                });
            }
        }

        // Generate indices
        for(int i = 0; i < stacks; ++i){
            const uint32_t base = (slices + 1) * i;

            for(int j = 0; j < slices; ++j){
                const uint32_t topLeft = base + j;
                const uint32_t topRight = base + (j + 1);
                const uint32_t bottomLeft = base + (slices + 1) + j;
                const uint32_t bottomRight = base + (slices + 1) + (j + 1);

                submesh.indices.push_back(topLeft);
                submesh.indices.push_back(topRight);
                submesh.indices.push_back(bottomRight);

                submesh.indices.push_back(topLeft);
                submesh.indices.push_back(bottomRight);
                submesh.indices.push_back(bottomLeft);
            }
        }

        model.submeshes.push_back(submesh);

        // Compute AABB
        model.bounds.min = Vec3{-radius, -radius, -radius};
        model.bounds.max = Vec3{ radius,  radius,  radius};

        return model;
    }

    static ModelData createEmbeddedPlane(){
        ModelData model;

        SubmeshData submesh;
        submesh.primitiveType = RHIPrimitiveTopology::TriangleList;
        submesh.materialSlotName = "default";

        // Plane in XZ plane (Y=0), 1x1 unit
        submesh.vertices = {
            Vertex{Vec3{-0.5f, 0.0f, -0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{0.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, 0.0f, -0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{1.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{ 0.5f, 0.0f,  0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
            Vertex{Vec3{-0.5f, 0.0f,  0.5f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{0.0f, 0.0f}, Vec4{1.0f, 0.0f, 0.0f, 1.0f}},
        };

        submesh.indices = {
            0, 2, 1,
            0, 3, 2
        };

        model.submeshes.push_back(submesh);

        // Compute AABB
        model.bounds.min = Vec3{-0.5f, 0.0f, -0.5f};
        model.bounds.max = Vec3{ 0.5f, 0.0f,  0.5f};

        return model;
    }

    static Vec4 toVec4(const aiColor4D& v){
        return {v.r, v.g, v.b, v.a};
    }

    // 3D Model Files to MeshData
    static std::optional<ModelData> loadModel(
        const std::string& path, RHICapabilities cap
    ){
        auto resolvedPath = resolveAssetPath(path).string();

        Assimp::Importer importer;

        // Configure post-processing flags
        unsigned int flags =
            aiProcess_Triangulate |              // Convert all primitives to triangles
            aiProcess_CalcTangentSpace |         // Calculate tangent vectors
            aiProcess_GenSmoothNormals |         // Generate smooth normals if missing
            aiProcess_JoinIdenticalVertices |    // Merge duplicate vertices
            aiProcess_ImproveCacheLocality |     // Reorder for better cache performance
            aiProcess_SortByPType |              // Split by primitive type
            aiProcess_FindInvalidData |          // Remove invalid data
            aiProcess_ValidateDataStructure;     // Validate the loaded data

        // Load the mesh file
        const aiScene* scene = importer.ReadFile(resolvedPath, flags);

        // Check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode){
            LOG_ERROR(LOG_RESOURCE, "Assimp Error: {}", importer.GetErrorString());
            return std::nullopt;
        }

        // We only support meshes with at least one mesh
        if(scene->mNumMeshes == 0){
            return std::nullopt;
        }

        // Print scene info
        LOG_INFO(LOG_RESOURCE, "=== Scene Info ===");
        LOG_INFO(LOG_RESOURCE, "  Meshes: {}", scene->mNumMeshes);
        LOG_INFO(LOG_RESOURCE, "  Materials: {}", scene->mNumMaterials);
        LOG_INFO(LOG_RESOURCE, "  Textures: {}", scene->mNumTextures);

        ModelData modelData;
        Vec3 globalMin{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        Vec3 globalMax{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        // Get the directory of the model file for resolving texture paths
        auto modelDir = std::filesystem::path(path).parent_path();

        // Process materials first
        for(unsigned int matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx){
            const aiMaterial* aiMat = scene->mMaterials[matIdx];

            MaterialRef material;

            // Get material name
            aiString matName;
            if(aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS && matName.length > 0){
                material.name = std::string(matName.C_Str());
            }
            else{
                material.name = "material_" + std::to_string(matIdx);
            }

            // Get base color factor
            aiColor4D baseColor;
            if(aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS){
                material.baseColor = toVec4(baseColor);
            }
            else if(aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS){
                material.baseColor = toVec4(baseColor);
            }

            // Print all texture types in this material
            LOG_DEBUG(LOG_RESOURCE, "  Material[{}] '{}' texture count:",
                matIdx, material.name
            );
            for(unsigned int texType = aiTextureType_NONE; texType < AI_TEXTURE_TYPE_MAX; texType++){
                unsigned int count = aiMat->GetTextureCount(static_cast<aiTextureType>(texType));
                if(count > 0){
                    LOG_DEBUG(LOG_RESOURCE, "    Type {}: {} texture(s)",
                        texType, count
                    );
                }
            }

            // Get diffuse/base color texture
            aiString texPath;
            if(aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS){
                TextureRef texRef;
                std::filesystem::path texFilePath = modelDir / texPath.C_Str();
                texRef.path = texFilePath.string();
                texRef.flags = combine(TextureFlags::SRGB, TextureFlags::GenerateMips);
                material.textures[TextureSemantic::BaseColor] = texRef;
                LOG_DEBUG(LOG_RESOURCE, "      -> Loaded DIFFUSE: {}", texPath.C_Str());
            }

            // Get normal texture
            if(aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS){
                TextureRef texRef;
                std::filesystem::path texFilePath = modelDir / texPath.C_Str();
                texRef.path = texFilePath.string();
                texRef.flags = TextureFlags::GenerateMips;
                material.textures[TextureSemantic::Normal] = texRef;
            }

            modelData.materials[material.name] = material;
        }

        LOG_DEBUG(LOG_RESOURCE, "\n=== Final Materials ===");
        LOG_DEBUG(LOG_RESOURCE, "Loaded {} materials", modelData.materials.size());
        for(const auto& [name, mat]: modelData.materials){
            LOG_DEBUG(LOG_RESOURCE, "  Material: '{}'", name);
        }

        // Process all meshes in the scene
        for(unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx){
            const aiMesh* aiMesh = scene->mMeshes[meshIdx];

            // Skip non-triangle meshes (should not happen with aiProcess_Triangulate)
            if(aiMesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE){
                continue;
            }

            SubmeshData submesh;
            submesh.primitiveType = RHIPrimitiveTopology::TriangleList;

            // Set material slot name
            if(aiMesh->mMaterialIndex < scene->mNumMaterials){
                aiMaterial* material = scene->mMaterials[aiMesh->mMaterialIndex];
                aiString materialName;
                if(material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS && materialName.length > 0){
                    submesh.materialSlotName = std::string(materialName.C_Str());
                }
                else{
                    submesh.materialSlotName = "material_" + std::to_string(aiMesh->mMaterialIndex);
                }
            }
            else{
                submesh.materialSlotName = "default";
            }

            // Extract vertices
            for(unsigned int i = 0; i < aiMesh->mNumVertices; ++i){
                Vertex vertex;

                // Position
                vertex.position.x = aiMesh->mVertices[i].x;
                vertex.position.y = aiMesh->mVertices[i].y;
                vertex.position.z = aiMesh->mVertices[i].z;

                // Update global AABB
                globalMin.x = std::min(globalMin.x, vertex.position.x);
                globalMin.y = std::min(globalMin.y, vertex.position.y);
                globalMin.z = std::min(globalMin.z, vertex.position.z);
                globalMax.x = std::max(globalMax.x, vertex.position.x);
                globalMax.y = std::max(globalMax.y, vertex.position.y);
                globalMax.z = std::max(globalMax.z, vertex.position.z);

                // Normal
                if(aiMesh->HasNormals()){
                    vertex.normal.x = aiMesh->mNormals[i].x;
                    vertex.normal.y = aiMesh->mNormals[i].y;
                    vertex.normal.z = aiMesh->mNormals[i].z;
                }
                else{
                    vertex.normal = Vec3{0.0f, 1.0f, 0.0f};
                }

                // Texture coordinates (use first UV channel)
                // Flip V coordinate for Metal/OpenGL (DirectX uses top-left origin, Metal uses bottom-left)
                if(aiMesh->HasTextureCoords(0)){
                    vertex.texCoord.x = aiMesh->mTextureCoords[0][i].x;
                    vertex.texCoord.y = cap.flipTextureV ?
                        1.0f - aiMesh->mTextureCoords[0][i].y :
                        aiMesh->mTextureCoords[0][i].y;
                }
                else {
                    vertex.texCoord = Vec2{0.0f, 0.0f};
                }

                // Tangent
                if(aiMesh->HasTangentsAndBitangents()){
                    vertex.tangent.x = aiMesh->mTangents[i].x;
                    vertex.tangent.y = aiMesh->mTangents[i].y;
                    vertex.tangent.z = aiMesh->mTangents[i].z;
                    vertex.tangent.w = 1.0f; // Handedness (always 1 for now)
                }
                else{
                    vertex.tangent = Vec4{1.0f, 0.0f, 0.0f, 1.0f};
                }

                submesh.vertices.push_back(vertex);
            }

            // Extract indices
            for(unsigned int i = 0; i < aiMesh->mNumFaces; ++i){
                const aiFace& face = aiMesh->mFaces[i];

                // Should always be 3 due to aiProcess_Triangulate
                if(face.mNumIndices == 3){
                    submesh.indices.push_back(face.mIndices[0]);
                    submesh.indices.push_back(face.mIndices[1]);
                    submesh.indices.push_back(face.mIndices[2]);
                }
            }

            LOG_DEBUG(LOG_RESOURCE, "  Submesh[{}]: {} verts, {} tris, material='{}'",
                meshIdx,
                submesh.vertexCount(),
                submesh.triangleCount(),
                submesh.materialSlotName
            );

            modelData.submeshes.push_back(submesh);
        }

        LOG_DEBUG(LOG_RESOURCE, "\n=== Summary ===");
        LOG_DEBUG(LOG_RESOURCE, "Total submeshes: {}", modelData.submeshes.size());
        LOG_DEBUG(LOG_RESOURCE, "Total materials: {}", modelData.materials.size());

        // Set global AABB
        modelData.bounds.min = globalMin;
        modelData.bounds.max = globalMax;

        return modelData;
    }

    static std::optional<ModelData> loadEmbeddedModel(const std::string& name){
        ModelData meshData;

        if(name == "cube")
            meshData = createEmbeddedCube();
        else if(name == "sphere")
            meshData = createEmbeddedSphere();
        else if(name == "plane")
            meshData = createEmbeddedPlane();
        else
            return std::nullopt;

        meshData.materials.emplace("default",
            MaterialRef{
                .name = "default",
                .type = MaterialType::PBR,
                .textures = {},  // 비어있음 → 텍스처 로드 안 함
                .baseColor = Vec4{0.8f, 0.8f, 0.8f, 1.0f},
                .roughness = 0.5f
            }
        );

        return meshData;
    }

    std::optional<ModelData> importModel(const std::string& uri, RHICapabilities cap){
        auto [scheme, path] = splitSchemeAndPath(uri);

        switch(scheme){
        case SchemeKind::File:
            return loadModel(path, cap);
        case SchemeKind::Embedded:
            return loadEmbeddedModel(path);
        default:
            LOG_DEBUG(LOG_RESOURCE, "met Undefined Scheme");
        }

        return std::nullopt;
    }
}
