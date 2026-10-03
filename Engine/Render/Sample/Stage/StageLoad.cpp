#include "StageLoad.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "StringUtil.hpp"

namespace
{
    using namespace Crowy;
    using Clock = std::chrono::steady_clock;

    // the pool stages each array 16-byte aligned
    constexpr u64 StagingAlign = 16;

    constexpr u64 alignUp(u64 value, u64 alignment) noexcept {
        return (value + alignment - 1) / alignment * alignment;
    }

    f64 secondsSince(Clock::time_point start) {
        return std::chrono::duration<f64>(Clock::now() - start).count();
    }

    // a file the stage reads must be there and must not be an LFS pointer
    void requireContent(const std::filesystem::path& file) {
        if(!std::filesystem::exists(file))
            throw std::runtime_error(std::format("stage: no file {}", file));
        if(isLfsPointer(file)) {
            throw std::runtime_error(std::format(
                "stage: {} is a Git LFS pointer; run git lfs pull in the content repository",
                file
            ));
        }
    }

    u64 stagingBytesOf(const MeshData& mesh) {
        return alignUp(mesh.vertices.size() * sizeof(Vertex), StagingAlign)
            + alignUp(mesh.indices.size() * sizeof(u32), StagingAlign);
    }

    // the OffsetAllocator rounds a request up to its bin but files a free
    // region rounded down, so a capacity that fits exactly can still refuse
    constexpr u32 withPoolSlack(u64 elements) noexcept {
        return static_cast<u32>(elements + elements / 8 + 1024);
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

        u64 draws = stage.document.quads.size();
        for(const auto& instance: stage.document.instances) {
            const auto model = std::ranges::find(
                stage.document.models,
                instance.model,
                &StageModel::id
            );
            draws += stage.models[static_cast<usize>(model - stage.document.models.begin())]
                .slots.size();
        }

        return StageCapacities{
            .vertices = withPoolSlack(vertices),
            .indices = withPoolSlack(indices),
            .materials = countStageMaterials(stage),
            .draws = static_cast<u32>(draws),
            .stagingBytes = staging
        };
    }
}

namespace Crowy
{
    bool isLfsPointer(const std::filesystem::path& file) {
        constexpr std::string_view Signature = "version https://git-lfs";

        std::ifstream stream(file, std::ios::binary);
        std::string head(Signature.size(), '\0');
        stream.read(head.data(), static_cast<std::streamsize>(head.size()));

        return stream.gcount() == static_cast<std::streamsize>(Signature.size())
            && head == Signature;
    }

    LoadedStage loadStage(const std::filesystem::path& root) {
        return loadStage(root, root / StageScenePath);
    }

    LoadedStage loadStage(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    ) {
        LoadedStage stage;

        auto start = Clock::now();
        requireContent(sceneFile);
        stage.document = loadStageDocument(root, sceneFile);
        for(const auto& quad: stage.document.quads) {
            if(quad.flipbook && !stage.sprites.contains(quad.flipbook->sprite)) {
                stage.sprites.emplace(
                    quad.flipbook->sprite,
                    loadStageSprite(stage.document, quad.flipbook->sprite)
                );
            }
        }
        stage.timings.documentSeconds = secondsSince(start);

        start = Clock::now();
        stage.models.reserve(stage.document.models.size());
        for(const auto& row: stage.document.models) {
            const auto file = resolveStagePath(stage.document, row.path);
            requireContent(file);
            stage.models.push_back(LoadModel(file));
        }
        stage.timings.modelSeconds = secondsSince(start);

        start = Clock::now();
        for(const auto& material: stage.document.materials) {
            if(stage.images.contains(material.texture))
                continue;

            const auto file = resolveStagePath(stage.document, material.texture);
            requireContent(file);
            stage.images.emplace(material.texture, LoadImage(file));
        }
        stage.timings.imageSeconds = secondsSince(start);

        stage.unitQuad = MakePlane(unitZ(), -unitX(), Vec2{0.5f, 0.5f});
        stage.capacities = capacitiesOf(stage);
        if(stage.capacities.stagingBytes > StageStagingBudget) {
            throw std::runtime_error(std::format(
                "stage: geometry stages {} bytes, past the {} the pool's one upload may use",
                stage.capacities.stagingBytes,
                StageStagingBudget
            ));
        }

        return stage;
    }

    u32 countStageMaterials(const LoadedStage& stage) {
        const auto& document = stage.document;

        // the channels an instance whose model has an emissive slot glows on
        std::set<Str> channels;
        for(const auto& instance: document.instances) {
            const auto model = std::ranges::find(document.models, instance.model, &StageModel::id);
            const auto& data = stage.models[static_cast<usize>(model - document.models.begin())];
            const auto emits = std::ranges::any_of(data.slots, [&](const ModelSlot& slot) {
                const auto material =
                    std::ranges::find(document.materials, slot.material, &StageMaterial::id);
                return material != document.materials.end() && material->emissive;
            });
            if(emits)
                channels.insert(instance.emissiveChannel);
        }

        return static_cast<u32>(1 + channels.size() + document.quads.size());
    }
}
