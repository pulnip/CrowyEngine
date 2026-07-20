#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "SkyboxFloor.hpp"

namespace Crowy
{
    SceneData MakeSkyboxFloor(f32 halfSize, f32 uvScale){
        std::vector<SceneObject> objects;
        objects.reserve(1);

        objects.push_back(SceneObject{
            .mesh = MakePlane(halfSize, uvScale),
            .transform = unitMat(),
            .material = Material{
                // the grid carries the colour, so the constant factor is white
                .albedo = {1.0f, 1.0f, 1.0f},
                .metallic = 0.0f,
                .roughness = 0.6f,
                .albedoMap = "Content/Assets/blender_uv_grid_2k.ktx2"
            }
        });

        return SceneData{
            .objects = std::move(objects),
            .skybox = "Content/Assets/skybox"
        };
    }
}
