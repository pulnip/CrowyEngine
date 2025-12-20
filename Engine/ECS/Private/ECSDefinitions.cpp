#include "ECSDefinitions.hpp"

namespace Crowy
{
    CameraType toCameraType(const std::string& text){
        static std::unordered_map<std::string, CameraType> text2camera = {
            {"MAINCAMERA", CameraType::MainCamera},
            { "SUBCAMERA",  CameraType::SubCamera},
        };
        auto upper = toUpper(text);
        auto it = text2camera.find(upper);
        if (it == text2camera.end()){
            return CameraType::UNKNOWN;
        }
        return it->second;
    }

    Projection toProjection(const std::string& text){
        static std::unordered_map<std::string, Projection> text2projection = {
            {"PERSPECTIVE", Projection::PERSPECTIVE},
            {"ORTHOGRAPHIC",  Projection::ORTHOGRAPHIC},
        };
        auto upper = toUpper(text);
        auto it = text2projection.find(upper);
        if (it == text2projection.end()){
            return Projection::UNKNOWN;
        }
        return it->second;
    }
}