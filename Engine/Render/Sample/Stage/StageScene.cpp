#include "StageScene.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <stdexcept>
#include <tuple>

#include "EnumUtil.hpp"
#include "Geometry/Overlap3D.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    namespace
    {
        using MeshKey = std::tuple<usize, Str>;
        using MeshCache = std::map<MeshKey, MeshHandle>;

        constexpr f32 radians(f32 degrees) noexcept {
            return degrees * (std::numbers::pi_v<f32> / 180.0f);
        }

        // the unit quad is a plane; a sliver of depth keeps its box whole
        constexpr AABB3D UnitQuadBounds{
            .center = {0.0f, 0.0f, 0.0f},
            .halfScale = {0.5f, 0.5f, 0.001f}
        };

        MaterialPipelineDesc opaquePipeline() {
            return MaterialPipelineDesc{
                .vertexShader = {.path = StageForwardShader, .entryPoint = "vs_main"},
                .fragmentShader = {.path = StageForwardShader, .entryPoint = "fs_opaque"},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        const StageMaterial& materialNamed(const StageDocument& document, StrView id) {
            return *std::ranges::find(document.materials, id, &StageMaterial::id);
        }

        TextureSampler samplerOf(StageSampler sampler) {
            return sampler == StageSampler::Point
                ? TextureSampler::NearestClamp
                : TextureSampler::LinearClamp;
        }

        bool showsIn(const StageNames& keys, StrView key) {
            return keys.empty() || std::ranges::find(keys, key) != keys.end();
        }

        f32 scaleNamed(const StageScales& scales, StrView name) {
            const auto found = std::ranges::find(scales, name, &StageScale::name);

            return found == scales.end() ? 0.0f : found->scale;
        }

        void setVisible(RenderScene& scene, PrimitiveHandle handle, bool visible) {
            auto& flags = scene.Primitives().GetRef(handle).flags;
            flags = visible
                ? combine(flags, PrimitiveFlags::Visible)
                : static_cast<PrimitiveFlags>(
                      static_cast<u32>(flags) & ~static_cast<u32>(PrimitiveFlags::Visible)
                  );
        }

        // one mesh per model, or per model and channel when it glows: the
        // emissive slot draws with that channel's material
        MeshHandle meshOf(
            RenderScene& scene,
            const LoadedStage& stage,
            const StageGeometry& geometry,
            const StageBindings& bindings,
            MeshCache& cache,
            usize model,
            StrView channel
        ) {
            const auto& data = stage.models[model];
            const auto emits = std::ranges::any_of(data.slots, [&](const ModelSlot& slot) {
                return materialNamed(stage.document, slot.material).emissive;
            });
            const MeshKey key{model, emits ? Str(channel) : Str{}};
            if(const auto found = cache.find(key); found != cache.end())
                return found->second;

            MeshResource mesh{.localBounds = data.bounds};
            for(usize slot = 0; slot < data.slots.size(); ++slot) {
                const auto& material = materialNamed(stage.document, data.slots[slot].material);
                mesh.materials.push_back(
                    material.emissive ? bindings.emissivePalettes.at(Str(channel)) : bindings.palette
                );
                mesh.subMeshes.push_back(SubMesh{
                    .geometry = geometry.models[model][slot],
                    .localBounds = data.slots[slot].bounds,
                    .materialSlot = static_cast<u32>(slot)
                });
            }

            return cache[key] = scene.Meshes().Add(std::move(mesh));
        }

        MaterialHandle addQuadMaterial(
            RenderScene& scene,
            const StageDocument& document,
            const StageQuad& quad,
            const StageTextureHandles& textures
        ) {
            const auto& atlas = materialNamed(document, quad.material);
            const auto texture = textures.at(atlas.texture);

            MaterialResource material{
                .data = {
                    .albedo = ones(),
                    .roughness = StageRoughness,
                    .uvScaleOffset = {
                        quad.uv1.x - quad.uv0.x,
                        quad.uv1.y - quad.uv0.y,
                        quad.uv0.x,
                        quad.uv0.y
                    },
                    .flags = atlas.receivesShadows
                        ? 0u
                        : static_cast<u32>(MaterialFlags::NoShadowReceive)
                },
                .pipeline = opaquePipeline(),
                .maps = {.albedo = texture}
            };
            if(!quad.emissiveChannel.empty())
                material.maps.emissive = texture;

            return scene.Materials().Add(std::move(material));
        }

        // the nearest any instance's box comes to `point`
        f32 nearestDistance(const LoadedStage& stage, Vec3 point) {
            auto nearest = 1e30f;
            const auto& document = stage.document;
            for(const auto& instance: document.instances) {
                const auto model = std::ranges::find(document.models, instance.model, &StageModel::id);
                const auto& local = stage.models[static_cast<usize>(model - document.models.begin())].bounds;
                const auto box = transformAABB3D(instanceToWorld(instance), local);
                const auto outside = Vec3{
                    std::max(0.0f, std::abs(point.x - box.center.x) - box.halfScale.x),
                    std::max(0.0f, std::abs(point.y - box.center.y) - box.halfScale.y),
                    std::max(0.0f, std::abs(point.z - box.center.z) - box.halfScale.z)
                };
                nearest = std::min(nearest, norm(outside));
            }

            return nearest;
        }
    }

    Mat4 instanceToWorld(const StageInstance& instance) {
        return translateMat(instance.position)
            * rotateYMat(radians(instance.yaw))
            * scaleMat(instance.scale);
    }

    Mat4 quadToWorld(const StageQuad& quad) {
        return translateMat(quad.position)
            * rotateYMat(radians(quad.yaw))
            * scaleMat({quad.width, quad.height, 1.0f});
    }

    StageGeometry addStageGeometry(GeometryPool& pool, const LoadedStage& stage) {
        StageGeometry geometry;
        geometry.models.reserve(stage.models.size());
        for(const auto& model: stage.models) {
            auto& slots = geometry.models.emplace_back();
            for(const auto& slot: model.slots)
                slots.push_back(pool.Add(slot.mesh.vertices, slot.mesh.indices));
        }
        geometry.unitQuad = pool.Add(stage.unitQuad.vertices, stage.unitQuad.indices);

        return geometry;
    }

    StageTextureHandles uploadStageTextures(
        RenderScene& scene,
        RHIDevice& device,
        const LoadedStage& stage
    ) {
        StageTextureHandles textures;
        for(const auto& material: stage.document.materials) {
            if(textures.contains(material.texture))
                continue;

            const auto& image = stage.images.at(material.texture);
            textures.emplace(
                material.texture,
                scene.Textures().Add(TextureResource{
                    .texture = device.CreateTexture(
                        RHITextureCreateDesc{
                            .width = image.width,
                            .height = image.height,
                            .mipLevels = image.mipLevels,
                            .format = image.format,
                            .usage = RHITextureUsage::ShaderRead,
                            .initialData = image.subs
                        },
                        material.id
                    ),
                    .sampler = samplerOf(material.sampler)
                })
            );
        }

        return textures;
    }

    StageBindings populateStage(
        RenderScene& scene,
        const LoadedStage& stage,
        const StageGeometry& geometry,
        const StageTextureHandles& textures
    ) {
        const auto& document = stage.document;
        StageBindings bindings;

        // every material row before any mesh names one; the palette is the
        // models' material that does not glow
        const StageMaterial* paletteRow = nullptr;
        for(const auto& model: document.models) {
            for(const auto& id: model.materials) {
                const auto& row = materialNamed(document, id);
                if(!row.emissive && paletteRow == nullptr)
                    paletteRow = &row;
            }
        }
        if(paletteRow == nullptr)
            throw std::runtime_error("stage: no model draws with a material that does not glow");
        const auto paletteTexture = textures.at(paletteRow->texture);
        bindings.palette = scene.Materials().Add(MaterialResource{
            .data = {.albedo = ones(), .roughness = StageRoughness},
            .pipeline = opaquePipeline(),
            .maps = {.albedo = paletteTexture}
        });
        for(const auto& instance: document.instances) {
            if(bindings.emissivePalettes.contains(instance.emissiveChannel))
                continue;
            bindings.emissivePalettes.emplace(
                instance.emissiveChannel,
                scene.Materials().Add(MaterialResource{
                    .data = {.albedo = ones(), .roughness = StageRoughness},
                    .pipeline = opaquePipeline(),
                    .maps = {.albedo = paletteTexture, .emissive = paletteTexture}
                })
            );
        }
        for(const auto& quad: document.quads)
            bindings.quadMaterials.push_back(addQuadMaterial(scene, document, quad, textures));

        MeshCache meshes;
        for(const auto& instance: document.instances) {
            const auto model = static_cast<usize>(
                std::ranges::find(document.models, instance.model, &StageModel::id) - document.models.begin()
            );
            const auto mesh = meshOf(scene, stage, geometry, bindings, meshes, model, instance.emissiveChannel);
            const auto localToWorld = instanceToWorld(instance);
            bindings.instances.push_back(scene.Primitives().Add(PrimitiveSnapshot{
                .localToWorld = localToWorld,
                .worldBounds = transformAABB3D(localToWorld, stage.models[model].bounds),
                .mesh = mesh
            }));
        }

        for(usize i = 0; i < document.quads.size(); ++i) {
            const auto mesh = scene.Meshes().Add(MeshResource{
                .subMeshes = {SubMesh{.geometry = geometry.unitQuad, .localBounds = UnitQuadBounds}},
                .materials = {bindings.quadMaterials[i]},
                .localBounds = UnitQuadBounds
            });
            const auto localToWorld = quadToWorld(document.quads[i]);
            bindings.quads.push_back(scene.Primitives().Add(PrimitiveSnapshot{
                .localToWorld = localToWorld,
                .worldBounds = transformAABB3D(localToWorld, UnitQuadBounds),
                .mesh = mesh
            }));
        }

        for(const auto& light: document.lights) {
            bindings.lights.push_back(scene.Lights().Add(LightSnapshot{
                .kind = light.kind == StageLightKind::Spot ? LightKind::Spot : LightKind::Point,
                .enabled = false,
                .color = light.color,
                .intensity = 0.0f,
                .position = light.position,
                .direction = normSquared(light.direction) > 0.0f ? normalize(light.direction) : -unitY(),
                .range = light.range,
                .innerConeAngle = radians(light.innerAngle),
                .outerConeAngle = radians(light.outerAngle)
            }));
        }
        bindings.sun = scene.Lights().Add(LightSnapshot{.enabled = false});

        return bindings;
    }

    Color applyStageKey(
        RenderScene& scene,
        const StageBindings& bindings,
        const StageDocument& document,
        usize key
    ) {
        const auto& lighting = document.lightingKeys.at(key);

        auto& sun = scene.Lights().GetRef(bindings.sun);
        sun.enabled = true;
        sun.castShadow = lighting.sunShadows;
        sun.direction = normalize(lighting.sunDirection);
        sun.color = lighting.sunColor;
        sun.intensity = lighting.sunIntensity;

        scene.Environment() = EnvironmentSnapshot{
            .skyAmbient = lighting.ambientSky * lighting.ambientIntensity,
            .groundAmbient = lighting.ambientGround * lighting.ambientIntensity
        };

        for(usize i = 0; i < document.lights.size(); ++i) {
            const auto& row = document.lights[i];
            const auto scale = scaleNamed(lighting.lightGroups, row.group);
            auto& light = scene.Lights().GetRef(bindings.lights[i]);
            light.enabled = scale > 0.0f;
            light.intensity = row.intensity * scale;
        }

        const auto glow = [&](StrView channel) {
            return lighting.emissiveScale * scaleNamed(lighting.emissiveChannels, channel) * ones();
        };
        for(const auto& [channel, material]: bindings.emissivePalettes)
            scene.Materials().GetRef(material).data.emissive = glow(channel);
        for(usize i = 0; i < document.quads.size(); ++i) {
            const auto& quad = document.quads[i];
            if(!quad.emissiveChannel.empty())
                scene.Materials().GetRef(bindings.quadMaterials[i]).data.emissive = glow(quad.emissiveChannel);
            setVisible(scene, bindings.quads[i], showsIn(quad.keys, lighting.name));
        }
        for(usize i = 0; i < document.instances.size(); ++i)
            setVisible(scene, bindings.instances[i], showsIn(document.instances[i].keys, lighting.name));

        const auto sky = lighting.skyHorizon;

        return Color{sky.x, sky.y, sky.z, 1.0f};
    }

    usize stageKeyIndex(const StageDocument& document, StrView name) {
        const auto found = std::ranges::find(document.lightingKeys, name, &StageLightingKey::name);
        if(found == document.lightingKeys.end()) {
            Str names;
            for(const auto& key: document.lightingKeys)
                names += names.empty() ? key.name : ", " + key.name;
            throw std::runtime_error(std::format("no lighting key '{}'; there are {}", name, names));
        }

        return static_cast<usize>(found - document.lightingKeys.begin());
    }

    StageCuts makeStageCuts(const LoadedStage& stage) {
        StageCuts cuts;
        for(const auto& camera: stage.document.cameras) {
            // half the gap to the nearest thing, within Unity's 0.3 and a
            // 1 m that keeps 5 mm layers apart at the establishing distance
            const auto nearZ = std::clamp(0.5f * nearestDistance(stage, camera.position), 0.3f, 1.0f);
            cuts.push_back(StageCut{
                .name = camera.name,
                .position = camera.position,
                .yaw = radians(camera.yaw),
                .pitch = -radians(camera.pitch),
                .fovY = radians(camera.fov),
                .orthographic = camera.orthographic,
                .orthoHalfHeight = camera.orthoSize,
                .nearZ = nearZ
            });
        }

        return cuts;
    }
}
