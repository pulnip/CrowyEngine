#include <format>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <toml++/toml.hpp>
#include "Log.hpp"
#include "SceneParser.hpp"

namespace Crowy{
    static const VNode* findField(const ValueArena& a,
        const VTable& table, const char* key
    ){
        auto it = table.fields.find(key);
        if(it == table.fields.end())
            return nullptr;
    
        return &a.nodes[it->second];
    }

    static std::optional<float> asFloat(const VNode& n){
        if (auto f = std::get_if<VFloat>(&n))
            return static_cast<float>(f->v);
        if (auto i = std::get_if<VInt>(&n))  
            return static_cast<float>(i->v);
        return std::nullopt;
    }

    static SourceLocation getLoc(const VNode& n){
        return std::visit(
            [](auto const& x) -> SourceLocation {
                return x.location;
            }, n);
    }

    static std::optional<bool> readBool(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<bool> def = std::nullopt
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto bl = std::get_if<VBool>(n))
            return bl->v;

        plan.errors.push_back({
            std::format("{} should be boolean", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    static std::optional<double> readFloat(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<double> def = std::nullopt
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto flt = std::get_if<VFloat>(n))
            return flt->v;
        else if(auto num = std::get_if<VInt>(n))
            return num->v;

        plan.errors.push_back({
            std::format("{} should be number", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    template<unsigned N>
    static std::optional<std::conditional_t<N==3, Vec3, Vec4>> readVec(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<std::conditional_t<N==3, Vec3, Vec4>> def = std::nullopt
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto arr = std::get_if<VArray>(n)){
            if(arr->elements.size() != N){
                plan.errors.push_back({
                    std::format("{} should be {}", key, N),
                    arr->location
                });
                return std::nullopt;
            }

            std::conditional_t<N==3, Vec3, Vec4> v;
            for(int i=0; i<N; ++i){
                const VNode& elem = arena.nodes[arr->elements[i]];
                auto f = asFloat(elem);
                if(!f){
                    plan.errors.push_back({
                        "element of Vec should be number",
                        getLoc(elem)
                    });
                    return std::nullopt;
                }
                v[i] = *f;
            }

            return v;
        }

        plan.errors.push_back({
            std::format("{} should be array", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    static std::optional<std::string> readString(
        const ValueArena& arena, const VTable& table,
        BindPlan& plan, const char* key,
        std::optional<std::string> def = std::nullopt
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto str = std::get_if<VString>(n))
            return str->v;

        plan.errors.push_back({
            std::format("{} should be string", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    // transform binder
    class TransformBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            auto pos = readVec<3>(arena, src, plan, "position", zeros());
            auto rot = readVec<4>(arena, src, plan, "rotation", unitQuat());
            auto scl = readVec<3>(arena, src, plan, "scale", ones());

            if(!pos || !rot || !scl)
                return;

            TransformDescriptor desc{
                .position = *pos,
                .rotation = *rot,
                .scale = *scl
            };
            plan.transforms.push_back(PlannedTransform{
                .desc = desc,
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.transforms){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushTransform(p.desc);
                entity.transformIndex = idx;
                entity.mask.set((size_t)ComponentKind::Transform);
            }
        }
    };

    class MeshBinder: public IComponentBinder{
    private:
        static std::optional<std::vector<MaterialDescriptor>> readMaterial(
            const ValueArena& arena, const VTable& src, BindPlan& plan
        ){
            if(const VNode* n = findField(arena, src, "material_override")){
                if(const VArray* arr = std::get_if<VArray>(n)){
                    std::vector<MaterialDescriptor> out;
                    out.reserve(arr->elements.size());
                    for(size_t idx: arr->elements){
                        const VNode& elm = arena.nodes[idx];

                        if(const VTable* t = std::get_if<VTable>(&elm)){
                            auto base = readString(arena, *t, plan, "baseColor");
                            auto tgt = readString(arena, *t, plan, "targetSlot");
                            if(!base || !tgt)
                                return std::nullopt;
    
                            out.push_back(MaterialDescriptor{
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
        static std::optional<ShaderDescriptor> readShader(
            const ValueArena& arena, const VTable& src, BindPlan& plan
        ){
            if(const VNode* n = findField(arena, src, "shader")){
                if(const VTable* mt = std::get_if<VTable>(n)){
                    auto mod = readString(arena, *mt, plan,
                        "module", "file:shader/ModernBoy.metallib");
                    auto vs = readString(arena, *mt, plan,
                        "vsFunc", "vertex_main");
                    auto fs = readString(arena, *mt, plan,
                        "fsFunc", "fragment_main");

                    if(!mod || !vs || !fs)
                        return std::nullopt;

                    ShaderDescriptor sd{
                        .module_ = *mod,
                        .vsFunc = *vs,
                        .fsFunc = *fs
                    };
                    return sd;
                } else{
                    plan.errors.push_back({"shader must be a table", getLoc(*n)});
                }
            }

            return std::nullopt;
        }

    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            auto msh = readString(arena, src, plan,
                "id", "embedded:cube");

            if(!msh)
                return;

            MeshDescriptor desc{};
            desc.id = *msh;

            if(auto mat = readMaterial(arena, src, plan))
                desc.material_override = *mat;

            if(auto sh = readShader(arena, src, plan))
                desc.shader = *sh;

            plan.meshes.push_back(PlannedMesh{
                .desc = desc,
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.meshes){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushMesh(p.desc);
                entity.meshIndex = idx;
                entity.mask.set((size_t)ComponentKind::Mesh);
            }
        }
    };

    class RigidbodyBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            auto vel = readVec<3>(arena, src, plan,
                "velocity", zeros());
            auto ug = readBool(arena, src, plan,
                "useGravity", false);
            auto ms = readFloat(arena, src, plan,
                "mass", 1);

            if(!vel || !ug || !ms)
                return;

            RigidbodyDescriptor desc{
                .velocity = *vel,
                .useGravity = *ug,
                .mass = *ms
            };

            plan.rigidbodies.push_back({
                .desc = desc,
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.rigidbodies){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushRigidbody(p.desc);
                entity.rigidbodyIndex = idx;
                entity.mask.set((size_t)ComponentKind::Rigidbody);
            }
        }
    };

    static std::optional<ColliderMaterialDescriptor> readColliderMaterial(
        const ValueArena& arena, const VTable& src, BindPlan& plan
    ){
        if(const VNode* n = findField(arena, src, "material")){
            if(const VTable* mt = std::get_if<VTable>(n)){
                auto bc = readFloat(arena, *mt, plan, "bounciness");
                auto fr = readFloat(arena, *mt, plan, "friction");

                if(!bc || !fr)
                    return std::nullopt;

                ColliderMaterialDescriptor cmd{
                    .bounciness = *bc,
                    .friction = *fr
                };
                return cmd;
            } else{
                plan.errors.push_back({"material must be a table", getLoc(*n)});
            }
        }

        return std::nullopt;
    }

    class BoxColliderBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            auto pos = readVec<3>(arena, src, plan, "position", zeros());
            auto rot = readVec<4>(arena, src, plan, "rotation", unitQuat());
            auto scl = readVec<3>(arena, src, plan, "scale", ones());

            if(!pos || !rot || !scl)
                return;

            BoxColliderDescriptor desc{};
            desc.position = *pos;
            desc.rotation = *rot;
            desc.scale = *scl;

            if(auto mat = readColliderMaterial(arena, src, plan))
                desc.material = *mat;

            plan.boxColliders.push_back({
                .desc = desc,
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.boxColliders){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushBoxCollider(p.desc);
                entity.boxColliderIndex = idx;
                entity.mask.set((size_t)ComponentKind::BoxCollider);
            }
        }
    };

    class SphereColliderBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            auto pos = readVec<3>(arena, src, plan, "position", zeros());
            auto rad = readFloat(arena, src, plan, "radius");

            if(!pos || !rad)
                return;

            SphereColliderDescriptor desc{};
            desc.position = *pos;
            desc.radius = *rad;

            if(auto mat = readColliderMaterial(arena, src, plan))
                desc.material = *mat;

            plan.sphereColliders.push_back({
                .desc = desc,
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.sphereColliders){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushSphereCollider(p.desc);
                entity.sphereColliderIndex = idx;
                entity.mask.set((size_t)ComponentKind::SphereCollider);
            }
        }
    };

    class CameraBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            auto tp = readString(arena, src, plan, "type");
            auto fv = readFloat(arena, src, plan, "fov");
            auto np = readFloat(arena, src, plan, "nearPlane", 0.1);
            auto fp = readFloat(arena, src, plan, "farPlane", 100.0);
            auto pj = readString(arena, src, plan, "projection");

            if(!tp || !fv || !np || !fp || !pj)
                return;

            CameraDescriptor desc{
                .type = *tp,
                .fov = *fv,
                .nearPlane = *np,
                .farPlane = *fp,
                .projection = *pj
            };

            plan.cameras.push_back({
                .desc = desc,
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.cameras){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushCamera(p.desc);
                entity.cameraIndex = idx;
                entity.mask.set((size_t)ComponentKind::Camera);
            }
        }
    };

    class PlayerBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            plan.players.push_back({
                .desc = {},
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.players){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushPlayer(p.desc);
                entity.playerIndex = idx;
                entity.mask.set((size_t)ComponentKind::Player);
            }
        }
    };

    class EditorBinder: public IComponentBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t entityIndex, BindPlan& plan
        ) override{
            plan.editors.push_back({
                .desc = {},
                .entityIndex = entityIndex,
                .location = src.location
            });
        }

        static void freeze(SceneDescriptor& desc, BindPlan& plan){
            for(const auto& p: plan.editors){
                auto& entity = desc.entities[p.entityIndex];

                const uint32_t idx = desc.pushEditor(p.desc);
                entity.editorIndex = idx;
                entity.mask.set((size_t)ComponentKind::Editor);
            }
        }
    };

    SourceLocation toSourceLocation(const toml::source_region& r){
        return SourceLocation{
            static_cast<size_t>(r.begin.line),
            static_cast<size_t>(r.begin.column)
        };
    }

    size_t convertTomlNodeToArena(const toml::node& n, ValueArena& arena);

    size_t convertTomlArrayToArena(const toml::array& arr, ValueArena& arena){
        VArray va{};
        va.location = toSourceLocation(arr.source());
        va.elements.reserve(arr.size());
        for(const toml::node& elem: arr){
            size_t childIdx = convertTomlNodeToArena(elem, arena);
            va.elements.push_back(childIdx);
        }
        return arena.emplace(std::move(va));
    }

    size_t convertTomlTableToArena(const toml::table& t, ValueArena& arena){
        VTable vt{};
        vt.location = toSourceLocation(t.source());
        for(auto&& [k, v]: t){
            size_t childIdx = convertTomlNodeToArena(v, arena);
            vt.fields.emplace(k.str(), childIdx);
        }
        return arena.emplace(std::move(vt));
    }

    size_t convertEntityComponentsToArena(const toml::table& et, ValueArena& arena){
        VTable vt{};
        vt.location = toSourceLocation(et.source());
        static const std::unordered_set<std::string> kReserved = {"name", "tags", "id", "uuid"};
        for(auto&& [k, v]: et){
            const auto key = std::string(k.str());
            if(kReserved.count(key)) continue;
            if(const toml::table* child = v.as_table()){
                size_t childIdx = convertTomlNodeToArena(*child, arena);
                vt.fields.emplace(key, childIdx);
            }
        }
        return arena.emplace(std::move(vt));
    }

    size_t convertTomlNodeToArena(const toml::node& n, ValueArena& arena){
        // Order: table, array, string, integer, floating, boolean, null
        if(auto t = n.as_table()){
            return convertTomlTableToArena(*t, arena);
        }
        if(auto a = n.as_array()){
            return convertTomlArrayToArena(*a, arena);
        }
        if(auto s = n.as_string()){
            VString vs{
                .v = std::string{s->get()},
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vs));
        }
        if(auto i = n.as_integer()){
            VInt vi{
                .v = i->get(),
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vi));
        }
        if(auto f = n.as_floating_point()){
            VFloat vf{
                .v = f->get(),
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vf));
        }
        if(auto b = n.as_boolean()){
            VBool vb{
                .v = b->get(),
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vb));
        }
        // Fallback: null node with best-effort location
        VNull vn{
            .location = toSourceLocation(n.source()) };
        return arena.emplace(std::move(vn));
    }

    TempScene parseSceneFromTable(const toml::table& root){
        TempScene out{};

        // Expect [[entities]] array of tables
        const toml::array* ents = root["entities"].as_array();
        if(!ents){
            // No entities; return empty scene (valid)
            return out;
        }

        out.entities.reserve(ents->size());

        size_t idx = 0;
        for(const toml::node& n : *ents){
            const toml::table* et = n.as_table();
            if(!et) continue;

            TempEntity e{};
            e.location = toSourceLocation(et->source());

            // name (optional)
            if(const auto* ns = (*et)["name"].as_string())
                e.name = std::string{ns->get()};
            else
                e.name = std::format("Entity{}", idx);

            // Treat direct child tables as components
            size_t compIdx = convertEntityComponentsToArena(*et, out.arena);
            e.componentsTableIndex = compIdx;

            out.entities.push_back(std::move(e));
            ++idx;
        }

        return out;
    }

    TempScene parseSceneFromFile(const std::string& sceneFile){
        toml::parse_result pr = toml::parse_file(sceneFile);
        if(pr.empty())
            return {};

        return parseSceneFromTable(*pr.as_table());
    }

    TempScene parseSceneFromString(std::string_view tomlText){
        toml::parse_result pr = toml::parse(tomlText);
        if(pr.empty())
            return {};

        return parseSceneFromTable(*pr.as_table());
    }

    SceneDescriptor buildScene(const TempScene& temp, const BinderRegistry& registry){
        SceneDescriptor out;
        // reserve entity slot and copy.
        out.entities.resize(temp.entities.size());
        for(size_t i=0;i<temp.entities.size();++i){
            out.entities[i].name = temp.entities[i].name;
        }

        // Binding (Validate & Plan)
        BindPlan plan;

        for(size_t ei = 0; ei < temp.entities.size(); ++ei){
            const auto& te = temp.entities[ei];
            const VNode& compsNode = temp.arena.nodes[te.componentsTableIndex];
            const VTable* comps = std::get_if<VTable>(&compsNode);
            if(!comps){
                plan.errors.push_back({"components must be table", te.location});
                continue;
            }

            for(const auto& kv : comps->fields){
                const std::string& compName = kv.first;
                size_t valueIdx = kv.second;

                auto it = registry.find(compName);
                if(it == registry.end()){
                    LOG_WARN(LOG_SCENE, "Unknown component '{}' on entity '{}'", compName, te.name);
                    continue;
                }

                const VNode& n = temp.arena.nodes[valueIdx];
                const VTable* compTbl = std::get_if<VTable>(&n);
                if(!compTbl){
                    plan.errors.push_back({
                        std::format("component '{}' must be a table", compName),
                        std::visit([](auto const& x){ return x.location; }, n)});
                    continue;
                }

                it->second->validateAndPlan(temp.arena, *compTbl, ei, plan);
            }
        }

        // Error Report
        if(!plan.errors.empty()){
            for(const auto& e: plan.errors){
                LOG_WARN(LOG_SCENE, "Scene bind error at {}:{} - {}", e.location.line, e.location.column, e.msg);
            }

            std::string all;
            all.reserve(plan.errors.size() * 64);
            for (const auto& e: plan.errors) {
                all += std::format("{}:{} - {}\n", e.location.line, e.location.column, e.msg);
            }
            throw std::runtime_error(all);
        }

        // Freeze(Create SoA + connect index)
        TransformBinder::freeze(out, plan);
        MeshBinder::freeze(out, plan);
        RigidbodyBinder::freeze(out, plan);
        BoxColliderBinder::freeze(out, plan);
        SphereColliderBinder::freeze(out, plan);
        CameraBinder::freeze(out, plan);
        PlayerBinder::freeze(out, plan);
        EditorBinder::freeze(out, plan);
        // Other ComponentBinder::freeze...

        return out;
    }

    BinderRegistry makeDefaultBinderRegistry(){
        BinderRegistry reg;
        reg.emplace("transform", std::make_unique<TransformBinder>());
        reg.emplace("mesh", std::make_unique<MeshBinder>());
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