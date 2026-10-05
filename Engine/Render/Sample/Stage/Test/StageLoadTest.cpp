#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "StageLoad.hpp"

using namespace Crowy;

TEST(StageLoad, RecognizesAnLfsPointer) {
    const auto directory = std::filesystem::temp_directory_path();
    const auto pointer = directory / "StageLoadTest_pointer.png";
    const auto real = directory / "StageLoadTest_real.png";
    const auto tiny = directory / "StageLoadTest_tiny.png";
    {
        std::ofstream(pointer, std::ios::binary)
            << "version https://git-lfs.github.com/spec/v1\noid sha256:00\nsize 268\n";
        std::ofstream(real, std::ios::binary) << "\x89PNG\r\n\x1a\n and more bytes than the signature";
        std::ofstream(tiny, std::ios::binary) << "ver";
    }

    EXPECT_TRUE(isLfsPointer(pointer));
    EXPECT_FALSE(isLfsPointer(real));
    EXPECT_FALSE(isLfsPointer(tiny));
    EXPECT_FALSE(isLfsPointer(directory / "StageLoadTest_missing.png"));

    std::filesystem::remove(pointer);
    std::filesystem::remove(real);
    std::filesystem::remove(tiny);
}

TEST(StageLoad, MissingRootThrowsNamingTheFile) {
    try {
        loadStage(std::filesystem::temp_directory_path() / "StageLoadTest_no_such_root");
        FAIL() << "expected a throw";
    }
    catch(const std::runtime_error& error) {
        EXPECT_NE(std::string_view(error.what()).find("scene.json"), std::string_view::npos);
    }
}
