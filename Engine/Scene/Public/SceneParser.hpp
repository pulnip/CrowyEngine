#pragma once

#include <bitset>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "math.hpp"

namespace Crowy
{
    // for toml::source_region
    struct SourceLocation{
        size_t line, column; 
    };

    struct VNull{
        SourceLocation location;
    };
    struct VBool{
        bool v;
        SourceLocation location;
    };
    struct VInt{
        int64_t v;
        SourceLocation location;
    };
    struct VFloat{
        double v;
        SourceLocation location;
    };
    struct VString{
        std::string v;
        SourceLocation location;
    };
    struct VArray{
        std::vector<size_t> elements;
        SourceLocation location;
    };
    struct VTable{
        std::unordered_map<std::string, size_t> fields;
        SourceLocation location;
    };

    using VNode = std::variant<
        VNull, VBool, VInt, VFloat,
        VString, VArray, VTable>;

    struct ValueArena{
        std::vector<VNode> nodes;
        inline size_t emplace(VNode n){
            nodes.push_back(std::move(n));
            return nodes.size() - 1;
        }
    };

    // parse result
    struct TempEntity{
        std::string name;
        size_t componentsTableIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct TempScene{
        ValueArena arena;
        std::vector<TempEntity> entities;
    };

    // SoA Intermediate Representation
    enum class ComponentKind: uint8_t{
        Transform = 0,
        Mesh = 1,
        Rigidbody = 2,
        BoxCollider = 3,
        SphereCollider = 4,
        Camera = 5,
        Player = 6,
        Editor = 7,
        Count = 8
    };
    constexpr auto INVALID = std::numeric_limits<uint32_t>::max();

    struct TransformDescriptor{
        Vec3 position;
        Vec4 rotation;
        Vec3 scale;
    };
    struct MaterialDescriptor{
        std::string baseColor;
        std::string targetSlot;
    };
    struct ShaderDescriptor{
        std::string module_;
        std::string vsFunc;
        std::string fsFunc;
    };
    struct MeshDescriptor{
        std::string id;
        std::vector<MaterialDescriptor> material_override;
        ShaderDescriptor shader;
    };
    struct RigidbodyDescriptor{
        Vec3 velocity;
        bool useGravity;
        double mass;
    };
    struct ColliderMaterialDescriptor{
        double bounciness;
        double friction;
    };
    struct BoxColliderDescriptor{
        Vec3 position;
        Vec4 rotation;
        Vec3 scale;
        ColliderMaterialDescriptor material;
    };
    struct SphereColliderDescriptor{
        Vec3 position;
        double radius;
        ColliderMaterialDescriptor material;
    };
    struct CameraDescriptor{
        std::string type;
        double fov;
        double nearPlane, farPlane;
        std::string projection;
    };
    struct PlayerDescriptor{};
    struct EditorDescriptor{};

        using TransformDescriptors = std::vector<TransformDescriptor>;
    using MaterialDescriptors = std::vector<MaterialDescriptor>;
    using MeshDescriptors = std::vector<MeshDescriptor>;
    using RigidbodyDescriptors = std::vector<RigidbodyDescriptor>;
    using BoxColliderDescriptors = std::vector<BoxColliderDescriptor>;
    using SphereColliderDescriptors = std::vector<SphereColliderDescriptor>;
    using CameraDescriptors = std::vector<CameraDescriptor>;
    using PlayerDescriptors = std::vector<PlayerDescriptor>;
    using EditorDescriptors = std::vector<EditorDescriptor>;

    struct EntityDescriptor{
        std::string name;
        std::bitset<(size_t)8> mask;
        uint32_t transformIndex = INVALID;
        uint32_t meshIndex = INVALID;
        uint32_t rigidbodyIndex = INVALID;
        uint32_t boxColliderIndex = INVALID;
        uint32_t sphereColliderIndex = INVALID;
        uint32_t cameraIndex = INVALID;
        uint32_t playerIndex = INVALID;
        uint32_t editorIndex = INVALID;
    };

    using EntityDescriptors = std::vector<EntityDescriptor>;

    struct SceneDescriptor{
        // SoA
        TransformDescriptors transforms;
        MeshDescriptors meshes;
        RigidbodyDescriptors rigidbodies;
        BoxColliderDescriptors boxColliders;
        SphereColliderDescriptors sphereColliders;
        CameraDescriptors cameras;
        PlayerDescriptors players;
        EditorDescriptors editors;

        EntityDescriptors entities;

        inline uint32_t pushTransform(const TransformDescriptor& desc){
            transforms.push_back(desc);
            return static_cast<uint32_t>(transforms.size() - 1);
        }
        inline uint32_t pushMesh(const MeshDescriptor& desc){
            meshes.push_back(desc);
            return static_cast<uint32_t>(meshes.size() - 1);
        }
        inline uint32_t pushRigidbody(const RigidbodyDescriptor& desc){
            rigidbodies.push_back(desc);
            return static_cast<uint32_t>(rigidbodies.size() - 1);
        }
        inline uint32_t pushBoxCollider(const BoxColliderDescriptor& desc){
            boxColliders.push_back(desc);
            return static_cast<uint32_t>(boxColliders.size() - 1);
        }
        inline uint32_t pushSphereCollider(const SphereColliderDescriptor& desc){
            sphereColliders.push_back(desc);
            return static_cast<uint32_t>(sphereColliders.size() - 1);
        }
        inline uint32_t pushCamera(const CameraDescriptor& desc){
            cameras.push_back(desc);
            return static_cast<uint32_t>(cameras.size() - 1);
        }
        inline uint32_t pushPlayer(const PlayerDescriptor& desc){
            players.push_back(desc);
            return static_cast<uint32_t>(players.size() - 1);
        }
        inline uint32_t pushEditor(const EditorDescriptor& desc){
            editors.push_back(desc);
            return static_cast<uint32_t>(editors.size() - 1);
        }
    };
    // bind/freeze plan
    struct BindError{
        std::string msg;
        SourceLocation location;
    };
    struct PlannedTransform{
        TransformDescriptor desc;
        // bind transform to entity by index
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedMesh{
        MeshDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedRigidbody{
        RigidbodyDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedBoxCollider{
        BoxColliderDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedSphereCollider{
        SphereColliderDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedCamera{
        CameraDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedPlayer{
        PlayerDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedEditor{
        EditorDescriptor desc;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };

    struct BindPlan{
        std::vector<PlannedTransform> transforms;
        std::vector<PlannedMesh> meshes;
        std::vector<PlannedRigidbody> rigidbodies;
        std::vector<PlannedBoxCollider> boxColliders;
        std::vector<PlannedSphereCollider> sphereColliders;
        std::vector<PlannedCamera> cameras;
        std::vector<PlannedPlayer> players;
        std::vector<PlannedEditor> editors;
        std::vector<BindError> errors;
    };

    // component binder interface
    class IComponentBinder{
    public:
        virtual ~IComponentBinder() = default;
        virtual void validateAndPlan(const ValueArena&,
            const VTable&, size_t entityIndex, BindPlan&)=0;
    };

    using BinderRegistry = std::unordered_map<std::string, std::unique_ptr<IComponentBinder>>;

    TempScene parseSceneFromFile(const std::string& sceneFile);
    TempScene parseSceneFromString(std::string_view tomlText);

    // Build (Bind + Freeze): TempScene → SceneDescriptor using registered binders
    SceneDescriptor buildScene(const TempScene& temp, const BinderRegistry& registry);

    // Convenience: default registry with built-in binders
    BinderRegistry makeDefaultBinderRegistry();
}