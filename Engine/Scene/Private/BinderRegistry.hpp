#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "Component.hpp"
#include "SceneSpec.hpp"
#include "SourceLocation.hpp"
#include "TempScene.hpp"

namespace Crowy
{
    /**
     * Component Binder System
     * 
     * only support internal Component for now,
     * if need to register UserComponent:
     * 1. move BinderRegistry to Public/
     * 2. use SceneParserPrivate.hpp instead SceneParser.hpp
     */

    // bind/freeze plan
    struct BindError{
        std::string msg;
        SourceLocation location;
    };
    struct PlannedTransform{
        TransformComponent comp;
        // bind transform to entity by index
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedMesh{
        MeshSpec spec;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedRigidbody{
        RigidbodyComponent comp;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedBoxCollider{
        BoxColliderComponent comp;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedSphereCollider{
        SphereColliderComponent comp;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedCamera{
        CameraComponent comp;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedPlayer{
        PlayerComponent comp;
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedEditor{
        EditorComponent comp;
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

    // Convenience: default registry with built-in binders
    BinderRegistry makeDefaultBinderRegistry();

    const VNode* findField(const ValueArena& a,
        const VTable& table, const char* key
    );
    SourceLocation getLoc(const VNode& n);

    std::optional<bool> readBool(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<bool> def = std::nullopt
    );
    std::optional<double> readFloat(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<double> def = std::nullopt
    );
    std::optional<Vec3> readVec3(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<Vec3> def = std::nullopt
    );
    std::optional<Vec4> readVec4(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<Vec4> def = std::nullopt
    );
    std::optional<std::string> readString(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<std::string> def = std::nullopt
    );
}