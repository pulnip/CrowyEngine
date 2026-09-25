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
