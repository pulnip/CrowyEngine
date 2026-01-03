#include <toml++/toml.hpp>
#include "ParserCommon.hpp"
#include "InputBinder.hpp"
#include "InputParser.hpp"

namespace Crowy
{
    InputSpec buildScene(const ParseResult& temp, const InputBinderRegistry& registry){
        InputSpec out;

        // TODO
        throw std::runtime_error("Not Implemented");

        auto plan = bindAndErrorReport(temp, registry);

        // Freeze(Create SoA + connect index)
        BindingsBinder::freeze(out, plan);

        return out;
    }

    InputSpec parseInputFromFile(std::string_view inputFile){
        auto binderRegistry = makeInputBinderRegistry();
        toml::parse_result pr = toml::parse_file(inputFile);
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