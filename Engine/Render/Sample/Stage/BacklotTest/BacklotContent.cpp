#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>

#include <gtest/gtest.h>

#include "BacklotContent.hpp"
#include "StageLoad.hpp"

namespace Crowy
{
    namespace
    {
        // a missing or LFS-less checkout skips the whole binary at once, so a
        // partial skip can never hide a failure beside it
        class BacklotContent final: public testing::Environment {
        public:
            void SetUp() override {
                for(const auto* relative: {
                        StageScenePath,
                        "Unity/Assets/Art/Textures/Palette.png",
                        "Unity/Assets/Art/Models/Probe/AxisProbe.fbx"
                    }) {
                    const auto file = backlotRoot() / relative;
                    if(!std::filesystem::exists(file) || isLfsPointer(file)) {
                        std::printf(
                            "skipped: %s is missing or a Git LFS pointer "
                            "(clone Backlot beside this repository, git lfs pull, "
                            "or set CROWY_BACKLOT_DIR)\n",
                            file.string().c_str()
                        );
                        std::fflush(stdout);
                        std::exit(77);
                    }
                }
            }
        };

        const auto* const content =
            testing::AddGlobalTestEnvironment(new BacklotContent);
    }

    std::filesystem::path backlotRoot() {
        return toPath(CROWY_BACKLOT_DIR);
    }
}
