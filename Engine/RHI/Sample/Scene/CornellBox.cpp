#include "CornellBox.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"

namespace Crowy
{
    namespace{
        // the canonical Cornell albedos, which are deliberately dim so that
        // colour bleeding between the walls stays visible
        constexpr Vec3 WHITE{0.73f, 0.73f, 0.73f};
        constexpr Vec3 RED{0.65f, 0.05f, 0.05f};
        constexpr Vec3 GREEN{0.12f, 0.45f, 0.15f};

        SceneObject Wall(Vec3 normal, Vec3 tangent, Vec3 center, f32 halfSize, Vec3 albedo){
            return SceneObject{
                .mesh = MakePlane(normal, tangent, halfSize),
                .transform = translateMat(center),
                .material = Material{
                    .albedo = albedo,
                    .metallic = 0.0f,
                    .roughness = 0.8f
                }
            };
        }

        SceneObject Sphere(Vec3 center, f32 radius, const Material& material){
            return SceneObject{
                .mesh = MakeSphere(radius, 48, 24),
                .transform = translateMat(center),
                .material = material
            };
        }
    }

    SceneData MakeCornellBox(f32 halfSize){
        const f32 h = halfSize;

        std::vector<SceneObject> objects;
        objects.reserve(9);

        // each wall faces into the room, so its normal points away from the
        // side it sits on
        objects.push_back(Wall({0.0f,  1.0f,  0.0f}, {1.0f, 0.0f,  0.0f}, {0.0f, -h, 0.0f}, h, WHITE));  // floor
        objects.push_back(Wall({0.0f, -1.0f,  0.0f}, {1.0f, 0.0f,  0.0f}, {0.0f,  h, 0.0f}, h, WHITE));  // ceiling
        objects.push_back(Wall({0.0f,  0.0f, -1.0f}, {1.0f, 0.0f,  0.0f}, {0.0f, 0.0f,  h}, h, WHITE));  // back
        objects.push_back(Wall({1.0f,  0.0f,  0.0f}, {0.0f, 0.0f,  1.0f}, {-h, 0.0f, 0.0f}, h, RED));    // left
        objects.push_back(Wall({-1.0f, 0.0f,  0.0f}, {0.0f, 0.0f, -1.0f}, { h, 0.0f, 0.0f}, h, GREEN));  // right

        // the spheres rest on the floor, so each sits one radius above it
        const f32 radius = 0.32f * h;
        const f32 y = -h + radius;

        objects.push_back(Sphere(
            {-0.52f*h, y, 0.30f*h}, radius,
            Material{
                .albedo = {0.95f, 0.93f, 0.88f},
                .metallic = 1.0f,
                .roughness = 0.06f
            }
        ));
        objects.push_back(Sphere(
            {0.02f*h, y, -0.22f*h}, radius,
            Material{
                .albedo = {0.80f, 0.28f, 0.26f},
                .metallic = 0.0f,
                .roughness = 0.35f
            }
        ));
        objects.push_back(Sphere(
            {0.55f*h, y, 0.36f*h}, radius,
            Material{
                .albedo = {0.72f, 0.56f, 0.30f},
                .metallic = 1.0f,
                .roughness = 0.42f
            }
        ));

        const AreaLight light{
            // just below the ceiling, so it does not z-fight with it
            .center = {0.0f, 0.985f*h, 0.06f*h},
            .tangent = {1.0f, 0.0f, 0.0f},
            .bitangent = {0.0f, 0.0f, 1.0f},
            .halfExtent = {0.26f*h, 0.26f*h},
            .normal = {0.0f, -1.0f, 0.0f},
            .radiance = {18.0f, 15.4f, 11.6f}
        };

        // the light is drawn as well, so a sample that only rasterizes albedo
        // still shows where it hangs
        objects.push_back(SceneObject{
            .mesh = MakePlane(light.normal, light.tangent, light.halfExtent.x),
            .transform = translateMat(light.center),
            .material = Material{
                .albedo = {0.0f, 0.0f, 0.0f},
                .emissive = light.radiance
            }
        });

        return SceneData{
            .objects = std::move(objects),
            .lights = {light}
        };
    }
}
