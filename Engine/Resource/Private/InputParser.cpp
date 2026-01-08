#include "path_util.hpp"
#include <toml++/toml.hpp>
#include "ParserCommon.hpp"
#include "InputBinder.hpp"
#include "InputParser.hpp"

namespace Crowy
{
    InputSpec buildScene(const ParseResult& temp, const InputBinderRegistry& registry){
        InputSpec out;
        // reserve pass slot and copy name.
        out.actions.resize(temp.elements.size());
        for(size_t i=0; i<temp.elements.size(); ++i){
            out.actions[i].name = temp.elements[i].name;
        }

        auto plan = bindAndErrorReport(temp, registry);

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