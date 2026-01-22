#include "path_util.hpp"
#include <toml++/toml.hpp>
#include "ParserCommon.hpp"
#include "InputBinder.hpp"
#include "InputParser.hpp"

namespace Crowy
{
    InputSpec buildScene(const ParseResult& temp, const InputBinderRegistry& registry){
        InputSpec out;
        InputElementBindPlan plan;
        // reserve pass slot and copy name.
        out.actions.resize(temp.elements.size());
        for(size_t i=0; i<temp.elements.size(); ++i){
            const auto& elm = temp.elements[i];
            const auto& node = temp.arena.nodes[elm.index];

            if(auto table = std::get_if<VTable>(&node)){
                auto name = readString(temp.arena, *table, plan.errors, "name");

                if(name.has_value())
                    out.actions[i].name = *name;
            }
            else{
                // TODO. write Error
            }                                                                  
        }

        bindAndErrorReport(temp, registry, plan);
        reportError(plan.errors);

        // Freeze(Create SoA + connect index)
        BindingsBinder::freeze(out, plan);

        return out;
    }

    InputSpec parseInputFromFile(std::filesystem::path inputFile){
        auto u8strPath = to_utf8String(inputFile);
        auto binderRegistry = makeInputBinderRegistry();
        toml::parse_result pr = toml::parse_file(u8strPath);
        if(pr.empty())
            return {};

        auto tempInput = parseFromTable(*pr.as_table(), "actions");
        return buildScene(tempInput, binderRegistry);
    }

    InputSpec parseInputFromString(std::string_view inputText){
        auto binderRegistry = makeInputBinderRegistry();
        toml::parse_result pr = toml::parse(inputText);
        if(pr.empty())
            return {};

        auto tempInput = parseFromTable(*pr.as_table(), "actions");
        return buildScene(tempInput, binderRegistry);
    }
}