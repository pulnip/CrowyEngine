#include <cstdio>
#include <exception>
#include <filesystem>
#include <print>

#include "BacklotImport.hpp"
#include "PathUtil.hpp"
#include "StageLoad.hpp"

namespace
{
    constexpr auto Usage =
        "usage: ImportBacklot [--backlot <dir>] [--out <dir>]\n"
        "  --backlot  Backlot's checkout (default " CROWY_BACKLOT_DIR ")\n"
        "  --out      where the content goes (default Content/Backlot)";
}

int main(int argc, char** argv) {
    using namespace Crowy;

    auto backlot = toPath(CROWY_BACKLOT_DIR);
    auto out = toPath("Content/Backlot");
    for(int i = 1; i < argc; i += 2) {
        const StrView flag = argv[i];
        if(i + 1 >= argc || (flag != "--backlot" && flag != "--out")) {
            std::println(stderr, "{}", Usage);

            return 2;
        }

        (flag == "--out" ? out : backlot) = toPath(argv[i + 1]);
    }

    try {
        if(const auto missing = missingStageContent(backlot)) {
            std::println(
                stderr,
                "{} is missing or a Git LFS pointer (clone Backlot beside "
                "this repository and git lfs pull, or pass --backlot)",
                toUTF8String(*missing)
            );

            return 1;
        }

        const auto imported = importBacklot(loadStage(backlot));
        writeBacklotImport(imported, out);
        std::print("{}", imported.report);
    }
    catch(const std::exception& error) {
        std::println(stderr, "import failed: {}", error.what());

        return 1;
    }

    return 0;
}
