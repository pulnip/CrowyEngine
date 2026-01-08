#include <unordered_map>
#include "string.hpp"
#include "ECSDefinitions.hpp"

namespace Crowy
{
    Projection toProjection(const std::string& text){
        static std::unordered_map<std::string, Projection> text2projection = {
            {"PERSPECTIVE", Projection::PERSPECTIVE},
            {"ORTHOGRAPHIC",  Projection::ORTHOGRAPHIC},
        };
        auto upper = toUpper(text);

        auto it = text2projection.find(upper);
        if(it == text2projection.end()){
            return Projection::UNKNOWN;
        }
        return it->second;
    }
}