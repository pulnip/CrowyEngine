#include "BacklotImport.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>

#include "AssetRef.hpp"
#include "CameraComponent.hpp"
#include "EnvironmentComponent.hpp"
#include "JsonLoader.hpp"
#include "LightComponent.hpp"
#include "LinearAlgebra.hpp"
#include "MaterialAsset.hpp"
#include "MeshComponent.hpp"
#include "PathUtil.hpp"
#include "RenderScene.hpp"
#include "Scene.hpp"
#include "StageScene.hpp"
#include "StringUtil.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        using Refs = std::vector<AssetRef>;

        constexpr CStr QuadMesh = "engine:Builtin/Quad";
        constexpr CStr MaterialFolder = "Materials";
        constexpr CStr PaletteFile = "Materials/Palette.json";
        // where the Unity project keeps them; the engine's copy drops it
        constexpr StrView ArtFolder = "Unity/Assets/Art/";

        f32 radians(f32 degrees) {
            return static_cast<f32>(toRadian(degrees));
        }

        // the content mount's ref of a file under the output folder
        AssetRef refOf(StrView relative) {
            return AssetRef::Parse(
                       std::format("{}/{}", BacklotFolder, relative)
            )
                .value();
        }

        Str contentPath(StrView path) {
            consumePrefix(path, ArtFolder);

            return Str(path);
        }

        bool showsIn(const StageNames& keys, StrView key) {
            return keys.empty() || std::ranges::contains(keys, key);
        }

        f32 scaleNamed(const StageScales& scales, StrView name) {
            const auto found =
                std::ranges::find(scales, name, &StageScale::name);

            return found == scales.end() ? 0.0f : found->scale;
        }

        TextureSampler samplerOf(StageSampler sampler) {
            return sampler == StageSampler::Point ? TextureSampler::NearestClamp
                                                  : TextureSampler::LinearClamp;
        }

        const StageMaterial& materialNamed(
            const StageDocument& document,
            StrView id
        ) {
            const auto found =
                std::ranges::find(document.materials, id, &StageMaterial::id);
            if(found == document.materials.end()) {
                throw std::runtime_error(
                    std::format("backlot: no material '{}' in the file", id)
                );
            }

            return *found;
        }

        // the row's image on a material, drawn as Stage draws it; `glow`
        // makes the image also the emission, at that value
        MaterialDesc texturedMaterial(
            const StageMaterial& row,
            Vec4 rect,
            std::optional<f32> glow
        ) {
            const MaterialTexture map{
                .texture = refOf(contentPath(row.texture)),
                .sampler = samplerOf(row.sampler)
            };
            const bool masked = row.kind == StageMaterialKind::Masked;
            MaterialDesc desc{
                .domain =
                    masked ? MaterialDomain::Masked : MaterialDomain::Opaque,
                .receiveShadow = row.receivesShadows,
                .data =
                    {.albedo = ones(),
                     .roughness = StageRoughness,
                     .uvScaleOffset = rect},
                .albedoMap = map
            };
            if(masked)
                desc.data.alphaCutoff = row.cutoff;
            if(glow) {
                desc.data.emissive = *glow * ones();
                desc.emissiveMap = map;
            }

            return desc;
        }

        Str textOf(const MaterialDesc& desc) {
            return emitJson(serializeMaterialDesc(desc), JsonStyle::Pretty) +
                   "\n";
        }

        Str quadMaterialFile(const StageQuad& quad) {
            if(quad.emissiveChannel.empty()) {
                return std::format(
                    "{}/{}_{}.json",
                    MaterialFolder,
                    quad.material,
                    quad.image
                );
            }

            return std::format(
                "{}/{}_{}_{}.json",
                MaterialFolder,
                quad.material,
                quad.image,
                quad.emissiveChannel
            );
        }

        Str emissivePaletteFile(StrView channel) {
            return std::format(
                "{}/PaletteEmissive_{}.json",
                MaterialFolder,
                channel
            );
        }

        // the file's own meters and degrees, as the components hold them
        Transform cameraTransform(const StageCut& cut) {
            return Transform{
                .position = cut.position,
                .rotation = quatOf(Vec3{cut.pitch, cut.yaw, 0.0f})
            };
        }

        LightComponent lightComponentOf(
            const StageLight& light,
            const StageLightingKey& key
        ) {
            const auto scale = scaleNamed(key.lightGroups, light.group);
            const bool on = scale > 0.0f;

            return LightComponent{
                .kind = light.kind == StageLightKind::Spot ? LightKind::Spot
                                                           : LightKind::Point,
                .enabled = on,
                .color = light.color,
                .intensity = on ? light.intensity * scale : light.intensity,
                .range = light.range,
                .innerConeAngle = radians(light.innerAngle),
                .outerConeAngle = radians(light.outerAngle)
            };
        }

        Transform lightTransform(const StageLight& light) {
            Transform transform{.position = light.position};
            if(light.kind == StageLightKind::Spot) {
                transform.rotation = lookRotation(
                    normSquared(light.direction) > 0.0f
                        ? normalize(light.direction)
                        : -unitY()
                );
            }

            return transform;
        }

        // the unit quad the engine builds faces -Z with u along +X, and
        // Stage's faces +Z with u along -X: the same quad turned half a turn
        Transform quadTransform(const StageQuad& quad) {
            return Transform{
                .position = quad.position,
                .rotation =
                    rotateY(radians(quad.yaw) + std::numbers::pi_v<f32>),
                .scale = {quad.width, quad.height, 1.0f}
            };
        }

        Transform instanceTransform(const StageInstance& instance) {
            return Transform{
                .position = instance.position,
                .rotation = rotateY(radians(instance.yaw)),
                .scale = instance.scale
            };
        }

        class Importer {
        private:
            const LoadedStage& stage;
            const StageDocument& document;
            const StageLightingKey& key;
            StageMaterialPlan plan;
            BacklotImport result;

            SystemRegistry noSystems;
            RenderScene scene;
            World world{noSystems};

            std::set<Str> textures;
            usize hiddenInstances = 0;
            usize hiddenQuads = 0;
            usize quadMaterials = 0;

        public:
            explicit Importer(const LoadedStage& stage)
                : stage(stage),
                  document(stage.document),
                  key(document.lightingKeys.at(
                      stageKeyIndex(document, document.defaultKey)
                  )),
                  plan(planStageMaterials(
                      document,
                      document.models,
                      stage.models
                  )) {
                world.Provide(scene);
                result.root = document.root;
            }

            BacklotImport Run() {
                addEnvironment();
                addCameras();
                addLights();
                addInstances();
                addQuads();
                addPalettes();
                addCopies();
                world.Frame(0.0);

                result.texts.emplace(
                    BacklotSceneFile,
                    emitJson(saveScene(world), JsonStyle::Pretty) + "\n"
                );
                result.report = reportOf();

                return std::move(result);
            }

        private:
            // the key's emissive value for a channel
            f32 glowOf(StrView channel) const {
                return key.emissiveScale *
                       scaleNamed(key.emissiveChannels, channel);
            }

            ActorId spawn(StrView name, const Transform& transform) {
                const auto actor = world.Spawn(transform);
                world.SetName(actor, Str(name));

                return actor;
            }

            void addText(const Str& path, Str text) {
                const auto [found, added] = result.texts.emplace(path, text);
                if(!added && found->second != text) {
                    throw std::runtime_error(
                        std::format(
                            "backlot: two rows would write {} differently",
                            path
                        )
                    );
                }
            }

            void addEnvironment() {
                const auto actor = spawn(
                    "environment",
                    Transform{
                        .rotation = lookRotation(normalize(key.sunDirection))
                    }
                );
                world.Add(
                    actor,
                    EnvironmentComponent{
                        .skyAmbient = key.ambientSky * key.ambientIntensity,
                        .groundAmbient =
                            key.ambientGround * key.ambientIntensity,
                        .skyColor = key.skyHorizon
                    }
                );
                world.Add(
                    actor,
                    LightComponent{
                        .kind = LightKind::Directional,
                        .castShadow = key.sunShadows,
                        .color = key.sunColor,
                        .intensity = key.sunIntensity
                    }
                );
            }

            void addCameras() {
                for(const auto& cut: makeStageCuts(stage)) {
                    world.Add(
                        spawn(cut.name, cameraTransform(cut)),
                        CameraComponent{
                            .fovY = cut.fovY,
                            .nearZ = cut.nearZ,
                            .farZ = cut.farZ
                        }
                    );
                }
            }

            void addLights() {
                for(const auto& light: document.lights) {
                    world.Add(
                        spawn(light.name, lightTransform(light)),
                        lightComponentOf(light, key)
                    );
                }
            }

            void addInstances() {
                for(const auto& instance: document.instances) {
                    const auto model = static_cast<usize>(
                        std::ranges::find(
                            document.models,
                            instance.model,
                            &StageModel::id
                        ) -
                        document.models.begin()
                    );

                    Refs materials;
                    for(const auto& slot: stage.models[model].slots) {
                        materials.push_back(
                            materialNamed(document, slot.material).emissive
                                ? refOf(emissivePaletteFile(
                                      instance.emissiveChannel
                                  ))
                                : refOf(PaletteFile)
                        );
                    }

                    const bool visible = showsIn(instance.keys, key.name);
                    hiddenInstances += visible ? 0 : 1;
                    world.Add(
                        spawn(instance.name, instanceTransform(instance)),
                        MeshComponent{
                            .mesh =
                                refOf(contentPath(document.models[model].path)),
                            .materials = std::move(materials),
                            .visible = visible
                        }
                    );
                }
            }

            void addQuads() {
                for(const auto& quad: document.quads) {
                    const auto& atlas = materialNamed(document, quad.material);
                    const auto file = quadMaterialFile(quad);
                    addText(
                        file,
                        textOf(texturedMaterial(
                            atlas,
                            stageQuadRect(quad, stage.sprites, 0.0),
                            quad.emissiveChannel.empty()
                                ? std::nullopt
                                : std::optional(glowOf(quad.emissiveChannel))
                        ))
                    );
                    textures.insert(atlas.texture);

                    const bool visible = showsIn(quad.keys, key.name);
                    hiddenQuads += visible ? 0 : 1;
                    world.Add(
                        spawn(quad.name, quadTransform(quad)),
                        MeshComponent{
                            .mesh = AssetRef::Parse(QuadMesh).value(),
                            .materials = {refOf(file)},
                            .visible = visible
                        }
                    );
                }
                quadMaterials = result.texts.size();
            }

            void addPalettes() {
                const auto& palette = *plan.palette;
                const Vec4 whole{1.0f, 1.0f, 0.0f, 0.0f};
                addText(
                    PaletteFile,
                    textOf(texturedMaterial(palette, whole, std::nullopt))
                );
                for(const auto& channel: plan.channels) {
                    addText(
                        emissivePaletteFile(channel),
                        textOf(
                            texturedMaterial(palette, whole, glowOf(channel))
                        )
                    );
                }
                textures.insert(palette.texture);
            }

            void addCopies() {
                std::map<Str, Str> toFrom;
                for(const auto& model: document.models)
                    toFrom.emplace(contentPath(model.path), model.path);
                for(const auto& texture: textures)
                    toFrom.emplace(contentPath(texture), texture);
                toFrom.emplace(BacklotSourceFile, StageScenePath);

                for(const auto& [to, from]: toFrom)
                    result.copies.push_back({.from = from, .to = to});
            }

            Str reportOf() const {
                std::vector<Str> lines;
                const auto line = [&lines](StrView text) {
                    lines.emplace_back(text);
                };

                const auto& name = key.name;
                line(std::format("backlot import: lighting key '{}'", name));
                line(
                    std::format(
                        "actors: 1 environment, {} cameras, {} lights, {} "
                        "instances, {} quads",
                        document.cameras.size(),
                        document.lights.size(),
                        document.instances.size(),
                        document.quads.size()
                    )
                );
                line(
                    std::format(
                        "materials: 1 palette, {} emissive palettes ({}), {} "
                        "quad materials",
                        plan.channels.size(),
                        join(plan.channels, ", "),
                        quadMaterials
                    )
                );
                line(
                    std::format(
                        "copies: {} models, {} images, the source scene",
                        document.models.size(),
                        textures.size()
                    )
                );
                line(
                    std::format(
                        "hidden in '{}' (visible:false): {} instances, {} "
                        "quads",
                        name,
                        hiddenInstances,
                        hiddenQuads
                    )
                );
                line(
                    std::format(
                        "lights off in '{}', intensity kept: {}",
                        name,
                        std::ranges::count_if(
                            document.lights,
                            [&](const StageLight& light) {
                                return scaleNamed(
                                           key.lightGroups,
                                           light.group
                                       ) <= 0.0f;
                            }
                        )
                    )
                );
                std::vector<Str> glows;
                for(const auto& channel: key.emissiveChannels) {
                    glows.push_back(
                        std::format("{} {}", channel.name, glowOf(channel.name))
                    );
                }
                line(
                    std::format(
                        "emission baked from '{}': {}",
                        name,
                        join(glows, ", ")
                    )
                );

                line("not imported:");
                std::vector<Str> others;
                for(const auto& other: document.lightingKeys) {
                    if(other.name != name)
                        others.push_back(other.name);
                }
                line(std::format("  lighting keys: {}", join(others, ", ")));
                line(
                    std::format(
                        "  light group and emissive channel scales as live "
                        "data: {} groups, {} channels",
                        key.lightGroups.size(),
                        key.emissiveChannels.size()
                    )
                );
                line(
                    std::format(
                        "  flipbook animation: {} quads, drawn at frame 0",
                        std::ranges::count_if(
                            document.quads,
                            [](const StageQuad& quad) {
                                return quad.flipbook.has_value();
                            }
                        )
                    )
                );
                line(
                    std::format(
                        "  detail: {} materials",
                        std::ranges::count_if(
                            document.materials,
                            [](const StageMaterial& material) {
                                return material.detail.has_value();
                            }
                        )
                    )
                );
                line(
                    std::format(
                        "  camera link: {} quads, drawn as stills",
                        std::ranges::count_if(
                            document.quads,
                            [](const StageQuad& quad) {
                                return !quad.camera.empty();
                            }
                        )
                    )
                );
                line("  sky_zenith, sky_haze");
                line(
                    std::format(
                        "  light shadow priority: {} lights ask for shadows, "
                        "none casts",
                        std::ranges::count_if(
                            document.lights,
                            [](const StageLight& light) {
                                return light.shadowPriority > 0;
                            }
                        )
                    )
                );
                for(const auto& cut: document.cameras) {
                    if(!cut.orthographic)
                        continue;

                    line(
                        std::format(
                            "  orthographic: '{}' (half height {} m) imported "
                            "as a perspective camera, fov {}",
                            cut.name,
                            cut.orthoSize,
                            cut.fov
                        )
                    );
                }

                return join(lines, "\n") + "\n";
            }
        };
    }

    BacklotImport importBacklot(const LoadedStage& stage) {
        return Importer(stage).Run();
    }

    void writeBacklotImport(
        const BacklotImport& imported,
        const std::filesystem::path& out
    ) {
        for(const auto& [path, text]: imported.texts) {
            const auto file = out / toPath(path);
            std::filesystem::create_directories(file.parent_path());

            std::ofstream stream(file, std::ios::binary);
            stream << text;
            if(!stream) {
                throw std::runtime_error(
                    std::format("cannot write {}", toUTF8String(file))
                );
            }
        }

        for(const auto& copy: imported.copies) {
            const auto to = out / toPath(copy.to);
            std::filesystem::create_directories(to.parent_path());
            std::filesystem::copy_file(
                imported.root / toPath(copy.from),
                to,
                std::filesystem::copy_options::overwrite_existing
            );
        }
    }
}
