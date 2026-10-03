#include <cstdio>
#include <filesystem>
#include <memory>

#include "FlyCamera.hpp"
#include "RenderApp.hpp"
#include "StageScene.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        std::filesystem::path backlotRoot() {
            return toPath(CROWY_BACKLOT_DIR);
        }

        // what the stage needs on disk; empty when it is all there
        Str missingContent() {
            for(const auto* relative: {StageScenePath, "Unity/Assets/Art/Textures/Palette.png"}) {
                const auto file = backlotRoot() / relative;
                if(!std::filesystem::exists(file) || isLfsPointer(file))
                    return toUTF8String(file);
            }

            return {};
        }
    }

    // Backlot's film set in the standard pipeline: loaded before RenderApp
    // exists, since the geometry pool is sized from it.
    class StageEditor final: public RenderApp {
    private:
        LoadedStage stage;
        StageCuts cuts;
        StageGeometry geometry;
        StageTextureHandles textures;
        StageBindings bindings;

    public:
        StageEditor()
            : StageEditor(loadStage(backlotRoot())) {}

        // the base reads `loaded` before the member takes it
        explicit StageEditor(LoadedStage loaded)
            : RenderApp(makeConfig(loaded), std::make_unique<FlyCamera>(makeCamera(loaded))),
              stage(std::move(loaded)),
              cuts(makeStageCuts(stage)) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            geometry = addStageGeometry(pool, stage);
        }

        void ExtractScene(RenderScene& scene) override {
            textures = uploadStageTextures(scene, Device(), stage);
            bindings = populateStage(scene, stage, geometry, textures);
            applyStageKey(
                scene,
                bindings,
                stage.document,
                stageKeyIndex(stage.document, stage.document.defaultKey)
            );
        }

    private:
        static Config makeConfig(const LoadedStage& loaded) {
            const auto& document = loaded.document;
            const auto& key = document.lightingKeys[stageKeyIndex(document, document.defaultKey)];
            const auto& capacities = loaded.capacities;

            return Config{
                .clearColor = {key.skyHorizon.x, key.skyHorizon.y, key.skyHorizon.z, 1.0f},
                .drawCapacity = capacities.draws,
                .materialCapacity = capacities.materials,
                // the fit spans the whole set and its 128 m grid
                .shadowMapSize = 4096,
                .vertexPoolCapacity = capacities.vertices,
                .indexPoolCapacity = capacities.indices
            };
        }

        // the scene file's first cut
        static FlyCamera::Config makeCamera(const LoadedStage& loaded) {
            const auto cut = makeStageCuts(loaded).front();

            return FlyCamera::Config{
                .position = cut.position,
                .yaw = cut.yaw,
                .pitch = cut.pitch,
                .fovY = cut.fovY,
                .nearZ = cut.nearZ,
                .farZ = cut.farZ,
                .moveSpeed = 15.0f
            };
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    // the content is another repository: without it there is nothing to show
    if(const auto missing = missingContent(); !missing.empty()) {
        std::printf(
            "skipped: %s is missing or a Git LFS pointer (clone Backlot beside "
            "this repository and git lfs pull, or set CROWY_BACKLOT_DIR)\n",
            missing.c_str()
        );
        return 77;
    }

    const WindowConfig windowConfig{
        .title = "StageEditor",
        .width = 1920,
        .height = 1080,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<StageEditor>(argc, argv, windowConfig);
}
