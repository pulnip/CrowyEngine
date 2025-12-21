#pragma once

#include "BinderRegistry.hpp"

namespace Crowy
{
    // transform binder
    class TransformBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class MeshBinder: public IComponentBinder{
    private:
        static std::optional<std::vector<MaterialSpec>> readMaterial(
            const ValueArena& arena, const VTable& src, BindPlan& plan
        );
        static std::optional<ShaderSpec> readShader(
            const ValueArena& arena, const VTable& src, BindPlan& plan
        );

    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class RigidbodyBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class BoxColliderBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class SphereColliderBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class CameraBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class PlayerBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };

    class EditorBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override;

        static void freeze(SceneSpec& spec, BindPlan& plan);
    };
}