#include "path_util.hpp"
#include <toml++/toml.hpp>
#include "ParserCommon.hpp"
#include "RenderPassBinder.hpp"
#include "RenderParser.hpp"

namespace Crowy
{
    RenderSpec buildScene(const ParseResult& temp, const RenderPassBinderRegistry& registry){
        RenderSpec out;
        // reserve pass slot and copy name.
        out.passes.resize(temp.elements.size());
        for(size_t i=0; i<temp.elements.size(); ++i){
            out.passes[i].name = temp.elements[i].name;
        }

        auto plan = bindAndErrorReport(temp, registry);

        // Freeze(Create SoA + connect index)
        ShaderBinder::freeze(out, plan);

        return out;
    }

    RenderSpec parseRenderFromFile(const std::filesystem::path& renderFile){
        auto u8strPath = to_utf8String(renderFile);
        auto binderRegistry = makeRenderPassBinderRegistry();
        toml::parse_result pr = toml::parse_file(u8strPath);
        if(pr.empty())
            return {};

        auto tempRender = parseFromTable(*pr.as_table(), "passes");
        return buildScene(tempRender, binderRegistry);
    }

    RenderSpec parseRenderFromString(std::string_view renderText){
        auto binderRegistry = makeRenderPassBinderRegistry();
        toml::parse_result pr = toml::parse(renderText);
        if(pr.empty())
            return {};

        auto tempRender = parseFromTable(*pr.as_table(), "passes");
        return buildScene(tempRender, binderRegistry);
    }
}