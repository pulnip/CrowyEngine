#pragma once

#include "BinderRegistry.hpp"
#include "SceneSpec.hpp"
#include "ScriptSpec.hpp"

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

    struct PlannedTransform{
        TransformComponent comp;
        // bind transform to entity by index
        size_t entityIndex = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct PlannedRenderObject{
        RenderObjectSpec spec;
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
    struct PlannedScript{
        ScriptInstanceSpec comp;
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

    struct ComponentBindPlan{
        std::vector<PlannedTransform> transforms;
        std::vector<PlannedRenderObject> renderObjects;
        std::vector<PlannedRigidbody> rigidbodies;
        std::vector<PlannedBoxCollider> boxColliders;
        std::vector<PlannedSphereCollider> sphereColliders;
        std::vector<PlannedCamera> cameras;
        std::vector<PlannedScript> scripts;
        std::vector<PlannedPlayer> players;
        std::vector<PlannedEditor> editors;
        std::vector<BindError> errors;
    };

    using ComponentBinder = Binder<ComponentBindPlan>;
    using ComponentBinderRegistry = BinderRegistry<ComponentBindPlan>;
    // Convenience: default registry with built-in binders
    ComponentBinderRegistry makeDefaultComponentBinderRegistry();

    // transform binder
    class TransformBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class RenderObjectBinder: public ComponentBinder{
    private:
        static std::optional<std::vector<MaterialSpec>> readMaterial(
            const ValueArena& arena, const VTable& src, ComponentBindPlan& plan
        );

    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class RigidbodyBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class BoxColliderBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class SphereColliderBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class CameraBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class ScriptBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class PlayerBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };

    class EditorBinder: public ComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, ComponentBindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, ComponentBindPlan& plan);
    };
}