#include "StageLoad.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

#include "EnumUtil.hpp"
#include "GeometryPool.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "MipChain.hpp"
#include "PathUtil.hpp"

namespace Crowy
{
    namespace
    {
        void requireContent(const std::filesystem::path& file) {
            if(!std::filesystem::exists(file))
                throw std::runtime_error(
                    std::format("stage: no file {}", file)
                );
            if(isLfsPointer(file)) {
                throw std::runtime_error(
                    std::format(
                        "stage: {} is a Git LFS pointer; run git lfs pull in "
                        "the content repository",
                        file
                    )
                );
            }
        }

        u64 stagingBytesOf(const MeshData& mesh) {
            return geometryStagingBytes(
                mesh.vertices.size(),
                mesh.indices.size()
            );
        }

        // the model `id` names in `table`, which `models` is parallel to
        const ModelData& modelOf(
            const StageModels& table,
            const StageModelData& models,
            StrView id
        ) {
            const auto row = std::ranges::find(table, id, &StageModel::id);

            return models[static_cast<usize>(row - table.begin())];
        }

        u32 countMaterials(
            const StageDocument& document,
            const StageMaterialPlan& plan
        ) {
            return static_cast<u32>(
                1 + plan.channels.size() + document.quads.size()
            );
        }

        u64 countDraws(
            const StageDocument& document,
            const StageModels& table,
            const StageModelData& models
        ) {
            u64 draws = document.quads.size();
            for(const auto& instance: document.instances)
                draws += modelOf(table, models, instance.model).slots.size();

            return draws;
        }

        StageCapacities capacitiesOf(const LoadedStage& stage) {
            u64 vertices = stage.unitQuad.vertices.size();
            u64 indices = stage.unitQuad.indices.size();
            u64 staging = stagingBytesOf(stage.unitQuad);
            for(const auto& model: stage.models) {
                for(const auto& slot: model.slots) {
                    vertices += slot.mesh.vertices.size();
                    indices += slot.mesh.indices.size();
                    staging += stagingBytesOf(slot.mesh);
                }
            }

            return StageCapacities{
                .vertices = geometryPoolCapacityFor(vertices),
                .indices = geometryPoolCapacityFor(indices),
                .materials = countStageMaterials(stage),
                .draws = countStageDraws(stage),
                .stagingBytes = staging
            };
        }
    }

    bool isLfsPointer(const std::filesystem::path& file) {
        constexpr std::string_view Signature = "version https://git-lfs";

        std::ifstream stream(file, std::ios::binary);
        std::string head(Signature.size(), '\0');
        stream.read(head.data(), static_cast<std::streamsize>(head.size()));

        return stream.gcount() ==
                   static_cast<std::streamsize>(Signature.size()) &&
               head == Signature;
    }

    std::optional<std::filesystem::path> missingStageContent(
        const std::filesystem::path& root
    ) {
        if(const auto scene = root / StageScenePath;
           !std::filesystem::exists(scene))
            return scene;
        if(const auto palette = root / "Unity/Assets/Art/Textures/Palette.png";
           std::filesystem::exists(palette) && isLfsPointer(palette))
            return palette;

        return std::nullopt;
    }

    LoadedStage loadStage(const std::filesystem::path& root) {
        return loadStage(root, root / StageScenePath);
    }

    LoadedStage loadStage(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    ) {
        LoadedStage stage;

        requireContent(sceneFile);
        stage.document = loadStageDocument(root, sceneFile);
        stage.sprites = loadStageSprites(stage.document);

        stage.models.reserve(stage.document.models.size());
        for(const auto& row: stage.document.models) {
            const auto file = resolveStagePath(stage.document, row.path);
            requireContent(file);
            stage.models.push_back(loadModel(file));
        }

        for(const auto& material: stage.document.materials) {
            if(stage.images.contains(material.texture))
                continue;

            stage.samplers.emplace(material.texture, material.sampler);
            const auto file =
                resolveStagePath(stage.document, material.texture);
            requireContent(file);
            auto image = LoadImage(file);
            // a nearest sampler still walks mips a texture has
            if(material.sampler == StageSampler::Linear)
                generateMipChain(image);
            stage.images.emplace(material.texture, std::move(image));
        }

        stage.unitQuad = makeStageUnitQuad();
        stage.capacities = capacitiesOf(stage);
        if(stage.capacities.stagingBytes > GeometryStagingBudget) {
            throw std::runtime_error(
                std::format(
                    "stage: geometry stages {} bytes, past the {} one frame's "
                    "pool uploads may use",
                    stage.capacities.stagingBytes,
                    GeometryStagingBudget
                )
            );
        }

        return stage;
    }

    StageSprites loadStageSprites(const StageDocument& document) {
        StageSprites sprites;
        for(const auto& quad: document.quads) {
            if(!quad.flipbook)
                continue;
            if(!sprites.contains(quad.flipbook->sprite))
                sprites.emplace(
                    quad.flipbook->sprite,
                    loadStageSprite(document, quad.flipbook->sprite)
                );

            const auto& animations =
                sprites.at(quad.flipbook->sprite).animations;
            if(!std::ranges::contains(
                   animations,
                   quad.flipbook->animation,
                   &StageSpriteAnimation::name
               )) {
                throw std::runtime_error(
                    std::format(
                        "stage: quad '{}' plays '{}', which {} does not hold",
                        quad.name,
                        quad.flipbook->animation,
                        quad.flipbook->sprite
                    )
                );
            }
        }

        return sprites;
    }

    void checkStageReload(
        const LoadedStage& launched,
        const StageDocument& document
    ) {
        const auto& models = launched.document.models;
        for(const auto& row: document.models) {
            const auto launch =
                std::ranges::find(models, row.id, &StageModel::id);
            const bool same =
                launch != models.end() && launch->path == row.path &&
                launch->materials == row.materials && launch->box == row.box;
            if(!same) {
                throw std::runtime_error(
                    std::format(
                        "stage: model '{}' reads {}, which the editor did not "
                        "load at launch; a restart loads it",
                        row.id,
                        row.path
                    )
                );
            }
        }
        for(const auto& material: document.materials) {
            const auto loaded = launched.samplers.find(material.texture);
            if(loaded == launched.samplers.end() ||
               loaded->second != material.sampler) {
                throw std::runtime_error(
                    std::format(
                        "stage: material '{}' samples {} {}, which the editor "
                        "did not upload so at launch; a restart loads it",
                        material.id,
                        material.texture,
                        stageName(material.sampler)
                    )
                );
            }
        }

        // the swap populates the file over the launch's model table, within
        // what the launch reserved
        const auto plan = planStageMaterials(document, models, launched.models);
        const auto materials = countMaterials(document, plan);
        const auto draws = countDraws(document, models, launched.models);
        const auto& reserved = launched.capacities;
        if(materials > reserved.materials || draws > reserved.draws) {
            throw std::runtime_error(
                std::format(
                    "stage: the file needs {} materials and {} draws a pass, "
                    "past the {} and {} the launch reserved; a restart "
                    "reserves them",
                    materials,
                    draws,
                    reserved.materials,
                    reserved.draws
                )
            );
        }
    }

    StageReload reloadStageDocument(
        const LoadedStage& launched,
        const std::filesystem::path& sceneFile
    ) {
        requireContent(sceneFile);
        auto document = loadStageDocument(launched.document.root, sceneFile);
        checkStageReload(launched, document);
        auto sprites = loadStageSprites(document);
        // instances name models by id: the launch's table, which the pool
        // follows, serves a file that drops or reorders rows
        document.models = launched.document.models;

        return StageReload{
            .document = std::move(document),
            .sprites = std::move(sprites)
        };
    }

    StageMaterialPlan planStageMaterials(
        const StageDocument& document,
        const StageModels& table,
        const StageModelData& models
    ) {
        const auto materialNamed = [&](StrView id) {
            return std::ranges::find(
                document.materials,
                id,
                &StageMaterial::id
            );
        };

        StageMaterialPlan plan;
        for(const auto& row: table) {
            for(const auto& id: row.materials) {
                const auto material = materialNamed(id);
                if(material == document.materials.end()) {
                    throw std::runtime_error(
                        std::format(
                            "stage: model '{}' draws with material '{}', which "
                            "the file no longer holds; a restart loads it",
                            row.id,
                            id
                        )
                    );
                }
                if(!material->emissive && plan.palette == nullptr)
                    plan.palette = &*material;
            }
        }
        if(plan.palette == nullptr)
            throw std::runtime_error(
                "stage: no model draws with a material that does not glow"
            );

        for(usize i = 0; i < table.size(); ++i) {
            bool glows = false;
            for(const auto& slot: models[i].slots) {
                const auto material = materialNamed(slot.material);
                if(material == document.materials.end()) {
                    throw std::runtime_error(
                        std::format(
                            "stage: model '{}' has a slot drawing with "
                            "material '{}', which the file does not hold",
                            table[i].id,
                            slot.material
                        )
                    );
                }
                glows = glows || material->emissive;
            }
            plan.glowingModels.push_back(glows);
        }
        for(const auto& instance: document.instances) {
            const auto row =
                std::ranges::find(table, instance.model, &StageModel::id);
            if(plan.glowingModels[static_cast<usize>(row - table.begin())])
                plan.channels.insert(instance.emissiveChannel);
        }

        return plan;
    }

    u32 countStageMaterials(const LoadedStage& stage) {
        return countMaterials(
            stage.document,
            planStageMaterials(
                stage.document,
                stage.document.models,
                stage.models
            )
        );
    }

    u32 countStageDraws(const LoadedStage& stage) {
        return static_cast<u32>(
            countDraws(stage.document, stage.document.models, stage.models)
        );
    }

    MeshData makeStageUnitQuad() {
        return MakePlane(unitZ(), -unitX(), Vec2{0.5f, 0.5f});
    }
}
