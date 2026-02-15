#include <toml++/toml.hpp>
#include "ConfigParser.hpp"
#include "ParserCommon.hpp"
#include "path_util.hpp"

namespace Crowy
{
    static ScriptSpec build(const ParseResult& temp){
        ScriptSpec out;
        std::vector<BindError> errors;

        out.modules.reserve(temp.elements.size());
        for(const auto& elm: temp.elements){
            const auto& node = temp.arena.nodes[elm.index];

            if(auto table = std::get_if<VTable>(&node)){
                auto name = readString(temp.arena, *table, errors, "name");
                if(!name.has_value())
                    continue;

                auto files = readPathArray(temp.arena, *table, errors, "files", {});

                out.modules.push_back({
                    .name = *name,
                    .files = std::move(files)
                });
            }
        }

        return out;
    }

    ScriptSpec parseScriptFromFile(const std::filesystem::path &scriptFile){
        auto u8strPath = to_utf8String(scriptFile);
        auto pr = toml::parse_file(u8strPath);
        if(pr.empty())
            return {};

        auto temp = parseFromTable(*pr.as_table(), "modules");
        return build(temp);
    }

    ScriptSpec parseScriptFromString(std::string_view scriptText){
        auto pr = toml::parse(scriptText);
        if(pr.empty())
            return {};

        auto temp = parseFromTable(*pr.as_table(), "modules");
        return build(temp);
    }
}