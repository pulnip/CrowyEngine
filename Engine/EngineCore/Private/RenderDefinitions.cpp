#include <unordered_map>
#include "string.hpp"
#include "RenderDefinitions.hpp"

namespace Crowy
{
    RenderType toRenderType(const std::string& str){
        static std::unordered_map<std::string, RenderType> text2render = {
            {     "OPAQUE", RenderType::Opaque},
            {"TRANSPARENT", RenderType::Transparent},
            {      "UNLIT", RenderType::Unlit},
        };
        auto upper = toUpper(str);

        auto it = text2render.find(upper);
        if(it == text2render.end()){
            throw std::runtime_error("enum parse error");
        }
        return it->second;
    }
}