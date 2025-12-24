#include "InternalComponentBinder.hpp"

namespace Crowy
{
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

    void TransformBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        auto pos = readVec3(arena, src, plan, "position", zeros());
        auto rot = readVec4(arena, src, plan, "rotation", unitQuat());
        auto scl = readVec3(arena, src, plan, "scale", ones());

        if(!pos || !rot || !scl)
            return;

        plan.transforms.push_back(PlannedTransform{
            .comp = TransformComponent{
                .position = *pos,
                .rotation = *rot,
                .scale = *scl
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void TransformBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.transforms){
            auto& entity = spec.entities[p.entityIndex];

            spec.transforms.push_back(p.comp);
            entity.transformIndex = static_cast<uint32_t>(spec.transforms.size() - 1);
            entity.mask.set((size_t)ComponentKind::Transform);
        }
    }

    std::optional<std::vector<MaterialSpec>> RenderObjectBinder::readMaterial(
        const ValueArena& arena, const VTable& src, BindPlan& plan
    ){
        if(const VNode* n = findField(arena, src, "material_override")){
            if(const VArray* arr = std::get_if<VArray>(n)){
                std::vector<MaterialSpec> out;
                out.reserve(arr->elements.size());
                for(size_t idx: arr->elements){
                    const VNode& elm = arena.nodes[idx];

                    if(const VTable* t = std::get_if<VTable>(&elm)){
                        auto base = readString(arena, *t, plan, "baseColor");
                        auto tgt = readString(arena, *t, plan, "targetSlot");
                        if(!base || !tgt)
                            return std::nullopt;

                        out.push_back(MaterialSpec{
                            .baseColor = *base,
                            .targetSlot = *tgt
                        });
                    } else{
                        plan.errors.push_back({"material_override entries must be tables", getLoc(elm)});
                    }
                }

                return out;
            } else{
                plan.errors.push_back({"material_override must be a array", getLoc(*n)});
            }
        }

        return std::nullopt;
    }
    std::optional<ShaderSpec> RenderObjectBinder::readShader(
        const ValueArena& arena, const VTable& src, BindPlan& plan
    ){
        if(const VNode* n = findField(arena, src, "shader")){
            if(const VTable* mt = std::get_if<VTable>(n)){
                auto mod = readString(arena, *mt, plan,
                    "module", "file:shader/Crowy.metallib");
                auto vs = readString(arena, *mt, plan,
                    "vsFunc", "vertex_main");
                auto fs = readString(arena, *mt, plan,
                    "fsFunc", "fragment_main");

                if(!mod || !vs || !fs)
                    return std::nullopt;

                ShaderSpec ss{
                    .module_ = *mod,
                    .vsFunc = *vs,
                    .fsFunc = *fs
                };
                return ss;
            } else{
                plan.errors.push_back({"shader must be a table", getLoc(*n)});
            }
        }

        return std::nullopt;
    }

    void RenderObjectBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        auto msh = readString(arena, src, plan, "uri", "embedded:cube");

        if(!msh)
            return;

        RenderObjectSpec spec{
            .uri = *msh
        };

        if(auto mat = readMaterial(arena, src, plan))
            spec.material_override = *mat;

        if(auto sh = readShader(arena, src, plan))
            spec.shaderSpec = *sh;

        plan.renderObjects.push_back({
            .spec = spec,
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void RenderObjectBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.renderObjects){
            auto& entity = spec.entities[p.entityIndex];

            spec.renderObjects.push_back(p.spec);
            entity.renderObjectIndex = static_cast<uint32_t>(spec.renderObjects.size() - 1);
            entity.mask.set((size_t)ComponentKind::Mesh);
        }
    }

    void RigidbodyBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        auto vel = readVec3(arena, src, plan, "velocity", zeros());
        auto ug = readBool(arena, src, plan, "useGravity", false);
        auto ms = readFloat(arena, src, plan, "mass", 1);

        if(!vel || !ug || !ms)
            return;

        plan.rigidbodies.push_back({
            .comp = RigidbodyComponent{
                .velocity = *vel,
                .useGravity = *ug,
                .mass = static_cast<float>(*ms)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void RigidbodyBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.rigidbodies){
            auto& entity = spec.entities[p.entityIndex];

            spec.rigidbodies.push_back(p.comp);
            entity.rigidbodyIndex = static_cast<uint32_t>(spec.rigidbodies.size() - 1);
            entity.mask.set((size_t)ComponentKind::Rigidbody);
        }
    }

    void BoxColliderBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        auto pos = readVec3(arena, src, plan, "position", zeros());
        auto rot = readVec4(arena, src, plan, "rotation", unitQuat());
        auto scl = readVec3(arena, src, plan, "scale", ones());

        auto bc = readFloat(arena, src, plan, "bounciness");
        auto fr = readFloat(arena, src, plan, "friction");

        if(!pos || !rot || !scl || !bc || !fr)
            return;

        plan.boxColliders.push_back({
            .comp = BoxColliderComponent{
                .position = *pos,
                .rotation = *rot,
                .scale = *scl,
                .bounciness = static_cast<float>(*bc),
                .friction = static_cast<float>(*fr)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void BoxColliderBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.boxColliders){
            auto& entity = spec.entities[p.entityIndex];

            spec.boxColliders.push_back(p.comp);
            entity.boxColliderIndex = static_cast<uint32_t>(spec.boxColliders.size() - 1);
            entity.mask.set((size_t)ComponentKind::BoxCollider);
        }
    }

    void SphereColliderBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        auto pos = readVec3(arena, src, plan, "position", zeros());
        auto rad = readFloat(arena, src, plan, "radius");

        auto bc = readFloat(arena, src, plan, "bounciness");
        auto fr = readFloat(arena, src, plan, "friction");

        if(!pos || !rad)
            return;

        plan.sphereColliders.push_back({
            .comp = SphereColliderComponent{
                .position = *pos,
                .radius = static_cast<float>(*rad),
                .bounciness = static_cast<float>(*bc),
                .friction = static_cast<float>(*fr)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void SphereColliderBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.sphereColliders){
            auto& entity = spec.entities[p.entityIndex];

            spec.sphereColliders.push_back(p.comp);
            entity.sphereColliderIndex = static_cast<uint32_t>(spec.sphereColliders.size() - 1);
            entity.mask.set((size_t)ComponentKind::SphereCollider);
        }
    }

    void CameraBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        auto tp = readString(arena, src, plan, "type");
        auto fv = readFloat(arena, src, plan, "fov");
        auto np = readFloat(arena, src, plan, "nearPlane", 0.1);
        auto fp = readFloat(arena, src, plan, "farPlane", 100.0);
        auto pj = readString(arena, src, plan, "projection");

        if(!tp || !fv || !np || !fp || !pj)
            return;

        plan.cameras.push_back({
            .comp = CameraComponent{
                .type = toCameraType(*tp),
                .fov = static_cast<float>(*fv),
                .nearPlane = static_cast<float>(*np),
                .farPlane = static_cast<float>(*fp),
                .proj = toProjection(*pj)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void CameraBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.cameras){
            auto& entity = spec.entities[p.entityIndex];

            spec.cameras.push_back(p.comp);
            entity.cameraIndex = static_cast<uint32_t>(spec.cameras.size() - 1);
            entity.mask.set((size_t)ComponentKind::Camera);
        }
    }

    void PlayerBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        plan.players.push_back({
            .comp = {},
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void PlayerBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.players){
            auto& entity = spec.entities[p.entityIndex];

            spec.players.push_back(p.comp);
            entity.playerIndex = static_cast<uint32_t>(spec.players.size() - 1);
            entity.mask.set((size_t)ComponentKind::Player);
        }
    }

    void EditorBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, BindPlan& plan
    ){
        plan.editors.push_back({
            .comp = {},
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void EditorBinder::freeze(SceneSpec& spec, BindPlan& plan){
        for(const auto& p: plan.editors){
            auto& entity = spec.entities[p.entityIndex];

            spec.editors.push_back(p.comp);
            entity.editorIndex = static_cast<uint32_t>(spec.editors.size() - 1);
            entity.mask.set((size_t)ComponentKind::Editor);
        }
    }

    BinderRegistry makeDefaultBinderRegistry(){
        BinderRegistry reg;
        reg.emplace("transform", std::make_unique<TransformBinder>());
        reg.emplace("renderObject", std::make_unique<RenderObjectBinder>());
        reg.emplace("rigidbody", std::make_unique<RigidbodyBinder>());
        reg.emplace("boxCollider", std::make_unique<BoxColliderBinder>());
        reg.emplace("sphereCollider", std::make_unique<SphereColliderBinder>());
        reg.emplace("camera", std::make_unique<CameraBinder>());
        reg.emplace("player", std::make_unique<PlayerBinder>());
        reg.emplace("editor", std::make_unique<EditorBinder>());

        // register Other Components...
        return reg;
    }
}