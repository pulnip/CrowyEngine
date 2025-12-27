#include <format>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <toml++/toml.hpp>
#include "InternalComponentBinder.hpp"
#include "Log.hpp"
#include "SceneParser.hpp"
#include "SceneParserPrivate.hpp"

namespace Crowy
{
    static SourceLocation toSourceLocation(const toml::source_region& r){
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

    SceneSpec buildScene(const TempScene& temp, const ComponentBinderRegistry& registry){
        SceneSpec out;
        // reserve entity slot and copy.
        out.entities.resize(temp.entities.size());
        for(size_t i=0;i<temp.entities.size();++i){
            out.entities[i].name = temp.entities[i].name;
        }

        // Binding (Validate & Plan)
        ComponentBindPlan plan;

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
        RenderObjectBinder::freeze(out, plan);
        RigidbodyBinder::freeze(out, plan);
        BoxColliderBinder::freeze(out, plan);
        SphereColliderBinder::freeze(out, plan);
        CameraBinder::freeze(out, plan);
        PlayerBinder::freeze(out, plan);
        EditorBinder::freeze(out, plan);
        // Other ComponentBinder::freeze...

        return out;
    }

    SceneSpec parseCustomSceneFromFile(std::string_view sceneFile,
        const ComponentBinderRegistry& binderRegistry
    ){
        toml::parse_result pr = toml::parse_file(sceneFile);
        if(pr.empty())
            return {};

        auto tempScene = parseSceneFromTable(*pr.as_table());
        return buildScene(tempScene, binderRegistry);
    }

    SceneSpec parseCustomSceneFromString(std::string_view tomlText,
        const ComponentBinderRegistry& binderRegistry
    ){
        toml::parse_result pr = toml::parse(tomlText);
        if(pr.empty())
            return {};

        auto tempScene = parseSceneFromTable(*pr.as_table());
        return buildScene(tempScene, binderRegistry);
    }

    SceneSpec parseSceneFromFile(std::string_view sceneFile){
        auto defaultBinderRegistry = makeDefaultComponentBinderRegistry();

        return parseCustomSceneFromFile(sceneFile, defaultBinderRegistry);
    }

    SceneSpec parseSceneFromString(std::string_view sceneText){
        auto defaultBinderRegistry = makeDefaultComponentBinderRegistry();

        return parseCustomSceneFromString(sceneText, defaultBinderRegistry);
    }
}