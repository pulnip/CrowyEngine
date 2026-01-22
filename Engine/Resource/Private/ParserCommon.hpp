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

    inline size_t convertElementsToArena(const toml::table& et, ValueArena& arena){
        VTable vt{
            .location = toSourceLocation(et.source())
        };

        for(auto&& [k, v]: et){
            size_t childIdx = convertTomlNodeToArena(v, arena);
            vt.fields.emplace(k.str(), childIdx);
        }
        return arena.emplace(std::move(vt));
    }

    inline size_t convertTomlNodeToArena(const toml::node& n, ValueArena& arena){
        // Order: table, array, string, integer, floating, boolean, null
        if(auto t = n.as_table()){
            return convertElementsToArena(*t, arena);
        }
        else if(auto a = n.as_array()){
            return convertTomlArrayToArena(*a, arena);
        }
        else if(auto s = n.as_string()){
            VString vs{
                .v = std::string{s->get()},
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vs));
        }
        else if(auto i = n.as_integer()){
            VInt vi{
                .v = i->get(),
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vi));
        }
        else if(auto f = n.as_floating_point()){
            VFloat vf{
                .v = f->get(),
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vf));
        }
        else if(auto b = n.as_boolean()){
            VBool vb{
                .v = b->get(),
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vb));
        }
        else{
            // Fallback: null node with best-effort location
            VNull vn{
                .location = toSourceLocation(n.source()) };
            return arena.emplace(std::move(vn));
        }
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

            // Treat direct child tables as elements
            size_t compIdx = convertElementsToArena(*et, out.arena);
            e.index = compIdx;

            out.elements.push_back(std::move(e));
            ++idx;
        }

        return out;
    }

    template<typename BindPlan>
    inline void bindAndErrorReport(const ParseResult& temp, const BinderRegistry<BindPlan>& registry,
        BindPlan& plan
    ){
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
                if(it == registry.end())
                    continue;

                const VNode& n = temp.arena.nodes[valueIdx];
                if(auto table = std::get_if<VTable>(&n)){
                    it->second->validateAndPlan(temp.arena, *table, ei, plan);
                }
                else if(auto array = std::get_if<VArray>(&n)){
                    it->second->validateAndPlanArray(temp.arena, *array, ei, plan);
                }
            }
        }
    }

    inline void reportError(std::span<const BindError> errors){
        if(errors.empty())
            return;

        std::string all;
        all.reserve(errors.size() * 64);

        for(const auto& e: errors){
            LOG_WARN(LOG_RESOURCE, "bind error at {}:{} - {}", e.location.line, e.location.column, e.msg);
            all += std::format("{}:{} - {}\n", e.location.line, e.location.column, e.msg);
        }

        throw std::runtime_error(all);
    }
}