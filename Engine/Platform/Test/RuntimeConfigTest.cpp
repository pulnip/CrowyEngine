#include <array>
#include <stdexcept>
#include <gtest/gtest.h>
#include "JsonLoader.hpp"
#include "RuntimeConfig.hpp"

using namespace Crowy;

TEST(RuntimeConfig, AppDocumentSetsTheBenchmarkAndVsync){
    const auto config = loadJson<RuntimeConfig>(R"({
        "metadata": {"version": 0, "name": "Bench", "type": "app"},
        "runtime": {
            "window": {"vsync": false},
            "benchmark": {
                "enabled": true,
                "warmup_frames": 10,
                "measure_frames": 20,
                "report_path": "bench/report.md",
                "frame_path": "bench/frames.csv"
            }
        }
    })");

    EXPECT_FALSE(config.window.vsync);
    EXPECT_TRUE(config.benchmark.enabled);
    EXPECT_EQ(config.benchmark.warmupFrames, 10u);
    EXPECT_EQ(config.benchmark.measureFrames, 20u);
    EXPECT_EQ(config.benchmark.reportPath, "bench/report.md");
    EXPECT_EQ(config.benchmark.framePath, "bench/frames.csv");
}

TEST(RuntimeConfig, AbsentKeysKeepTheirDefaults){
    const auto config =
        loadJson<RuntimeConfig>(R"({"metadata": {"type": "app"}})");
    const BenchmarkConfig defaults;

    EXPECT_TRUE(config.window.vsync);
    EXPECT_EQ(config.benchmark.enabled, defaults.enabled);
    EXPECT_EQ(config.benchmark.warmupFrames, defaults.warmupFrames);
    EXPECT_EQ(config.benchmark.measureFrames, defaults.measureFrames);
    EXPECT_TRUE(config.benchmark.reportPath.empty());
    EXPECT_TRUE(config.benchmark.framePath.empty());
}

TEST(RuntimeConfig, OtherDocumentTypesAreRefused){
    EXPECT_THROW(
        loadJson<RuntimeConfig>(R"({"metadata": {"type": "resources"}})"),
        std::runtime_error
    );
}

// a typo on the command line is loud, unlike an env var nobody reads
TEST(RuntimeConfig, CommandLineRefusesWhatItDoesNotKnow){
    RuntimeConfig config;

    static char misspelled[] = "--confg";
    EXPECT_THROW(
        applyCommandLine(config, std::array{misspelled}),
        std::runtime_error
    );

    static char configFlag[] = "--config";
    EXPECT_THROW(
        applyCommandLine(config, std::array{configFlag}),
        std::runtime_error
    );
}

TEST(RuntimeConfig, CommandLineTakesFrameCaptures) {
    RuntimeConfig config;
    EXPECT_TRUE(config.captures.empty());

    static char captureFlag[] = "--capture-frame";
    static char sixty[] = "60";
    static char first[] = "captures/a.bmp";
    static char ninety[] = "90";
    static char second[] = "captures/b.bmp";
    applyCommandLine(
        config,
        std::array{captureFlag, sixty, first, captureFlag, ninety, second}
    );
    ASSERT_EQ(config.captures.size(), 2u);
    EXPECT_EQ(config.captures[0].frame, 60u);
    EXPECT_EQ(config.captures[0].path, "captures/a.bmp");
    EXPECT_EQ(config.captures[1].frame, 90u);
    EXPECT_EQ(config.captures[1].path, "captures/b.bmp");
}

TEST(RuntimeConfig, CommandLineRefusesAFrameCaptureItCannotTake) {
    static char captureFlag[] = "--capture-frame";
    static char sixty[] = "60";
    static char zero[] = "0";
    static char ninety[] = "90";
    static char word[] = "sixty";
    static char path[] = "a.bmp";
    static char other[] = "b.bmp";

    const auto refused = [](auto... args) {
        RuntimeConfig config;
        EXPECT_THROW(
            applyCommandLine(config, std::array{args...}),
            std::runtime_error
        );
    };
    refused(captureFlag);
    refused(captureFlag, sixty);
    refused(captureFlag, zero, path);
    // the frame comes first, so a path where it goes is caught
    refused(captureFlag, path, sixty);
    refused(captureFlag, word, path);
    // two dumps of one frame, or into one file
    refused(captureFlag, sixty, path, captureFlag, sixty, other);
    refused(captureFlag, sixty, path, captureFlag, ninety, path);
}

TEST(RuntimeConfig, CommandLineTakesAnExitFrame) {
    RuntimeConfig config;
    EXPECT_FALSE(config.exitAfterFrame.has_value());

    static char exitFlag[] = "--exit-after-frame";
    static char sixty[] = "60";
    static char captureFlag[] = "--capture-frame";
    static char path[] = "a.bmp";
    applyCommandLine(
        config,
        std::array{exitFlag, sixty, captureFlag, sixty, path}
    );
    EXPECT_EQ(config.exitAfterFrame, 60u);
}

TEST(RuntimeConfig, CommandLineRefusesAnExitItCannotTake) {
    static char exitFlag[] = "--exit-after-frame";
    static char thirty[] = "30";
    static char zero[] = "0";
    static char word[] = "sixty";
    static char captureFlag[] = "--capture-frame";
    static char sixty[] = "60";
    static char path[] = "a.bmp";

    const auto refused = [](auto... args) {
        RuntimeConfig config;
        EXPECT_THROW(
            applyCommandLine(config, std::array{args...}),
            std::runtime_error
        );
    };
    refused(exitFlag);
    refused(exitFlag, zero);
    refused(exitFlag, word);
    // a capture past the exit would never be written; either order is caught
    refused(exitFlag, thirty, captureFlag, sixty, path);
    refused(captureFlag, sixty, path, exitFlag, thirty);
}

TEST(RuntimeConfig, CommandLineTakesHold){
    RuntimeConfig config;
    EXPECT_FALSE(config.hold);

    static char holdFlag[] = "--hold";
    applyCommandLine(config, std::array{holdFlag});
    EXPECT_TRUE(config.hold);

    // still loud about whatever follows it
    static char misspelled[] = "--confg";
    EXPECT_THROW(
        applyCommandLine(config, std::array{holdFlag, misspelled}),
        std::runtime_error
    );
}

// kept as given: Main resolves it against the cwd it was launched from
TEST(RuntimeConfig, CommandLineTakesTheProjectFolder) {
    RuntimeConfig config;
    EXPECT_FALSE(config.project);

    static char projectFlag[] = "--project";
    static char folder[] = "../MyGame";
    applyCommandLine(config, std::array{projectFlag, folder});

    ASSERT_TRUE(config.project);
    EXPECT_EQ(*config.project, std::filesystem::path("../MyGame"));

    RuntimeConfig missing;
    EXPECT_THROW(
        applyCommandLine(missing, std::array{projectFlag}),
        std::runtime_error
    );
}
