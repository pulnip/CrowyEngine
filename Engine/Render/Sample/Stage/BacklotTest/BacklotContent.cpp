#include "BacklotContent.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>

#include <gtest/gtest.h>

#include "PathUtil.hpp"
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
                if(const auto missing = missingStageContent(backlotRoot())) {
                    std::printf(
                        "skipped: %s is missing or a Git LFS pointer (clone "
                        "Backlot beside this repository, git lfs pull, or set "
                        "CROWY_BACKLOT_DIR)\n",
                        toUTF8String(*missing).c_str()
                    );
                    std::fflush(stdout);
                    std::exit(77);
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
