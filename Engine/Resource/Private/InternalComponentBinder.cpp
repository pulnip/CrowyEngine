#include "InternalComponentBinder.hpp"
#include "math.hpp"

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
        Script = 6,
        Player = 7,
        Editor = 8,
        Count = 9
    };

    void TransformBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto pos = readVec3(arena, src, plan.errors, "position", zeros());
        auto rot = readVec4(arena, src, plan.errors, "rotation", unit_quat());
        auto scl = readVec3(arena, src, plan.errors, "scale", ones());

        plan.transforms.push_back(PlannedTransform{
            .comp = TransformComponent{
                .position = pos,
                .rotation = rot,
                .scale = scl
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void TransformBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.transforms){
            auto& entity = spec.entities[p.entityIndex];

            spec.transformSpecs.push_back(p.comp);
            entity.transformIndex = static_cast<uint32_t>(spec.transformSpecs.size() - 1);
        }
    }

    std::optional<std::vector<MaterialSpec>> RenderObjectBinder::readMaterial(
        const ValueArena& arena, const VTable& src, ComponentBindPlan& plan
    ){
        if(const VNode* n = findField(arena, src, "material_override")){
            if(const VArray* arr = std::get_if<VArray>(n)){
                std::vector<MaterialSpec> out;
                out.reserve(arr->elements.size());
                for(size_t idx: arr->elements){
                    const VNode& elm = arena.nodes[idx];

                    if(const VTable* t = std::get_if<VTable>(&elm)){
                        auto base = readString(arena, *t, plan.errors, "baseColor");
                        auto tgt = readString(arena, *t, plan.errors, "targetSlot");
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

    void RenderObjectBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto msh = readString(arena, src, plan.errors, "uri", "embedded:cube");
        auto renderType = readString(arena, src, plan.errors, "renderType", "unlit");

        RenderObjectSpec spec{
            .uri = msh,
            .renderType = renderType
        };

        if(auto mat = readMaterial(arena, src, plan))
            spec.material_override = *mat;

        plan.renderObjects.push_back({
            .spec = spec,
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void RenderObjectBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.renderObjects){
            auto& entity = spec.entities[p.entityIndex];

            spec.renderObjectSpecs.push_back(p.spec);
            entity.renderObjectIndex = static_cast<uint32_t>(spec.renderObjectSpecs.size() - 1);
        }
    }

    void RigidbodyBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto vel = readVec3(arena, src, plan.errors, "velocity", zeros());
        auto ug = readBool(arena, src, plan.errors, "useGravity", false);
        auto ms = readFloat(arena, src, plan.errors, "mass", 1);

        plan.rigidbodies.push_back({
            .comp = RigidbodyComponent{
                .velocity = vel,
                .useGravity = ug,
                .mass = static_cast<float>(ms)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void RigidbodyBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.rigidbodies){
            auto& entity = spec.entities[p.entityIndex];

            spec.rigidbodySpecs.push_back(p.comp);
            entity.rigidbodyIndex = static_cast<uint32_t>(spec.rigidbodySpecs.size() - 1);
        }
    }

    void BoxColliderBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto pos = readVec3(arena, src, plan.errors, "position", zeros());
        auto rot = readVec4(arena, src, plan.errors, "rotation", unit_quat());
        auto scl = readVec3(arena, src, plan.errors, "scale", ones());

        auto bc = readFloat(arena, src, plan.errors, "bounciness");
        auto fr = readFloat(arena, src, plan.errors, "friction");

        if(!bc || !fr)
            return;

        plan.boxColliders.push_back({
            .comp = BoxColliderComponent{
                .position = pos,
                .rotation = rot,
                .scale = scl,
                .bounciness = static_cast<float>(*bc),
                .friction = static_cast<float>(*fr)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void BoxColliderBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.boxColliders){
            auto& entity = spec.entities[p.entityIndex];

            spec.boxColliderSpecs.push_back(p.comp);
            entity.boxColliderIndex = static_cast<uint32_t>(spec.boxColliderSpecs.size() - 1);
        }
    }

    void SphereColliderBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto pos = readVec3(arena, src, plan.errors, "position", zeros());
        auto rad = readFloat(arena, src, plan.errors, "radius");

        auto bc = readFloat(arena, src, plan.errors, "bounciness");
        auto fr = readFloat(arena, src, plan.errors, "friction");

        if(!rad)
            return;

        plan.sphereColliders.push_back({
            .comp = SphereColliderComponent{
                .position = pos,
                .radius = static_cast<float>(*rad),
                .bounciness = static_cast<float>(*bc),
                .friction = static_cast<float>(*fr)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void SphereColliderBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.sphereColliders){
            auto& entity = spec.entities[p.entityIndex];

            spec.sphereColliderSpecs.push_back(p.comp);
            entity.sphereColliderIndex = static_cast<uint32_t>(spec.sphereColliderSpecs.size() - 1);
        }
    }

    void CameraBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto fv = readFloat(arena, src, plan.errors, "fov");
        auto np = readFloat(arena, src, plan.errors, "nearPlane", 0.1);
        auto fp = readFloat(arena, src, plan.errors, "farPlane", 100.0);
        auto pj = readString(arena, src, plan.errors, "projection");

        if(!fv || !pj)
            return;

        plan.cameras.push_back({
            .comp = CameraComponent{
                .fov = static_cast<float>(*fv),
                .nearPlane = static_cast<float>(np),
                .farPlane = static_cast<float>(fp),
                .proj = toProjection(*pj),
                // TODO. parse later
                .viewport = RHIViewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    // for fullscreen
                    .width  = 0.0f,
                    .height = 0.0f,
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                }
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void CameraBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.cameras){
            auto& entity = spec.entities[p.entityIndex];

            spec.cameraSpecs.push_back(p.comp);
            entity.cameraIndex = static_cast<uint32_t>(spec.cameraSpecs.size() - 1);
        }
    }

    void ScriptBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        auto monoScripts = readStringArray(arena, src, plan.errors, "monoScripts");

        if(!monoScripts)
            return;

        plan.scripts.push_back({
            .comp = {
                .monoScripts = std::move(*monoScripts)
            },
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void ScriptBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.scripts){
            auto& entity = spec.entities[p.entityIndex];

            spec.scriptSpecs.push_back(p.comp);
            entity.scriptIndex = static_cast<uint32_t>(spec.scriptSpecs.size() - 1);
        }
    }

    void PlayerBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        plan.players.push_back({
            .comp = {},
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void PlayerBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.players){
            auto& entity = spec.entities[p.entityIndex];

            spec.playerSpecs.push_back(p.comp);
            entity.playerIndex = static_cast<uint32_t>(spec.playerSpecs.size() - 1);
        }
    }

    void EditorBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t entityIndex, ComponentBindPlan& plan
    ){
        plan.editors.push_back({
            .comp = {},
            .entityIndex = entityIndex,
            .location = src.location
        });
    }

    void EditorBinder::freeze(SceneSpec& spec, ComponentBindPlan& plan){
        for(const auto& p: plan.editors){
            auto& entity = spec.entities[p.entityIndex];

            spec.editorSpecs.push_back(p.comp);
            entity.editorIndex = static_cast<uint32_t>(spec.editorSpecs.size() - 1);
        }
    }

    ComponentBinderRegistry makeDefaultComponentBinderRegistry(){
        ComponentBinderRegistry reg;
        reg.emplace("transform", std::make_unique<TransformBinder>());
        reg.emplace("renderObject", std::make_unique<RenderObjectBinder>());
        reg.emplace("rigidbody", std::make_unique<RigidbodyBinder>());
        reg.emplace("boxCollider", std::make_unique<BoxColliderBinder>());
        reg.emplace("sphereCollider", std::make_unique<SphereColliderBinder>());
        reg.emplace("camera", std::make_unique<CameraBinder>());
        reg.emplace("script", std::make_unique<ScriptBinder>());
        reg.emplace("player", std::make_unique<PlayerBinder>());
        reg.emplace("editor", std::make_unique<EditorBinder>());

        // register Other Components...
        return reg;
    }
}