#include <cstddef>
#include <format>
#include <unordered_set>
#include <toml++/toml.hpp>
#include "BinderRegistry.hpp"
#include "Log.hpp"
#include "ParseResult.hpp"
#include "SourceLocation.hpp"

namespace Crowy
{
    inline SourceLocation toSourceLocation(const toml::source_region& r){
        return SourceLocation{
            static_cast<size_t>(r.begin.line),
            static_cast<size_t>(r.begin.column)
        };
    }

    inline size_t convertTomlNodeToArena(const toml::node& n, ValueArena& arena);

    inline size_t convertTomlArrayToArena(const toml::array& arr, ValueArena& arena){
        VArray va{};
        va.location = toSourceLocation(arr.source());
        va.elements.reserve(arr.size());
        for(const toml::node& elem: arr){
            size_t childIdx = convertTomlNodeToArena(elem, arena);
            va.elements.push_back(childIdx);
        }
        return arena.emplace(std::move(va));
    }

    inline size_t convertTomlTableToArena(const toml::table& t, ValueArena& arena){
        VTable vt{};
        vt.location = toSourceLocation(t.source());
        for(auto&& [k, v]: t){
            size_t childIdx = convertTomlNodeToArena(v, arena);
            vt.fields.emplace(k.str(), childIdx);
        }
        return arena.emplace(std::move(vt));
    }

    inline size_t convertElementsToArena(const toml::table& et, ValueArena& arena){
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
            else if(const toml::array* child = v.as_array()){
                size_t childIdx = convertTomlArrayToArena(*child, arena);
                vt.fields.emplace(key, childIdx);
            }
        }
        return arena.emplace(std::move(vt));
    }

    inline size_t convertTomlNodeToArena(const toml::node& n, ValueArena& arena){
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

    inline ParseResult parseFromTable(const toml::table& root, const char* rootName){
        ParseResult out{};

        // Expect [[rootName]] array of tables
        const toml::array* array = root[rootName].as_array();
        if(!array){
            // No root;
            return out;
        }

        out.elements.reserve(array->size());

        size_t idx = 0;
        for(const toml::node& n: *array){
            const toml::table* et = n.as_table();
            if(!et) continue;

            ParseElement e{};
            e.location = toSourceLocation(et->source());

            // name (optional)
            if(const auto* ns = (*et)["name"].as_string())
                e.name = std::string{ns->get()};

            // Treat direct child tables as components
            size_t compIdx = convertElementsToArena(*et, out.arena);
            e.index = compIdx;

            out.elements.push_back(std::move(e));
            ++idx;
        }

        return out;
    }

    template<typename BindPlan>
    inline BindPlan bindAndErrorReport(const ParseResult& temp, const BinderRegistry<BindPlan>& registry){
        // Binding (Validate & Plan)
        BindPlan plan;

        for(size_t ei = 0; ei < temp.elements.size(); ++ei){
            const auto& te = temp.elements[ei];
            const VNode& node = temp.arena.nodes[te.index];
            const VTable* table = std::get_if<VTable>(&node);
            if(!table){
                plan.errors.push_back({"elements must be table", te.location});
                continue;
            }

            for(const auto& kv: table->fields){
                const std::string& name = kv.first;
                size_t valueIdx = kv.second;

                auto it = registry.find(name);
                if(it == registry.end()){
                    LOG_WARN(LOG_SCENE, "Unknown '{}' on render pass '{}'", name, te.name);
                    continue;
                }

                const VNode& n = temp.arena.nodes[valueIdx];
                if(auto table = std::get_if<VTable>(&n)){
                    it->second->validateAndPlan(temp.arena, *table, ei, plan);
                }
                else if(auto array = std::get_if<VArray>(&n)){
                    it->second->validateAndPlanArray(temp.arena, *array, ei, plan);
                }
                else{
                    plan.errors.push_back({
                        std::format("element '{}' must be a table or array", name),
                        std::visit([](auto const& x){ return x.location; }, n)
                    });
                }

            }
        }

        // Error Report
        if(!plan.errors.empty()){
            for(const auto& e: plan.errors){
                LOG_WARN(LOG_SCENE, "bind error at {}:{} - {}", e.location.line, e.location.column, e.msg);
            }

            std::string all;
            all.reserve(plan.errors.size() * 64);
            for (const auto& e: plan.errors) {
                all += std::format("{}:{} - {}\n", e.location.line, e.location.column, e.msg);
            }
            throw std::runtime_error(all);
        }

        return plan;
    }
}