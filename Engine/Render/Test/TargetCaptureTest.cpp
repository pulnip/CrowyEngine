#include <array>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "TargetCapture.hpp"

using namespace Crowy;

namespace
{
    using Bytes = std::vector<u8>;

    // the little-endian bytes of these values, one after another
    template<typename T>
    Bytes BytesOf(std::initializer_list<T> values) {
        Bytes bytes(values.size() * sizeof(T));
        std::memcpy(bytes.data(), values.begin(), bytes.size());

        return bytes;
    }

    Bytes Convert(
        RHIPixelFormat format,
        const Bytes& bytes,
        u32 width,
        u32 height,
        u32 rowPitch
    ) {
        Bytes rgba;
        EXPECT_TRUE(
            convertCapture(format, bytes, width, height, rowPitch, rgba)
        );

        return rgba;
    }

    // a 2 x 1 RGBA8 target in a row padded to 256 bytes
    TargetReadback Readback(u64 requested, u64 recorded, Str path) {
        auto buffer = std::make_unique<FakeBuffer>(256);
        const std::array<u8, 8> texels{1, 2, 3, 4, 5, 6, 7, 8};
        std::memcpy(buffer->bytes.data(), texels.data(), texels.size());

        return TargetReadback{
            .request =
                {.frame = requested,
                 .target = "SceneColor",
                 .path = std::move(path)},
            .target = 3,
            .recorded = recorded,
            .buffer = std::move(buffer),
            .format = RHIPixelFormat::RGBA8_UNORM,
            .width = 2,
            .height = 1,
            .rowPitch = 256
        };
    }
}

TEST(TargetCapture, Rgba8PassesThroughWithoutItsPadding) {
    constexpr u8 Pad = 0xEE;
    // 2 x 2, rows of 8 bytes in a pitch of 12
    const Bytes bytes{
        1, 2,  3,  4,  5,  6,  7,  8,  Pad, Pad, Pad, Pad,
        9, 10, 11, 12, 13, 14, 15, 16, Pad, Pad, Pad, Pad,
    };

    for(const auto format:
        {RHIPixelFormat::RGBA8_UNORM, RHIPixelFormat::RGBA8_UNORM_SRGB}) {
        EXPECT_EQ(
            Convert(format, bytes, 2, 2, 12),
            (Bytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16})
        );
    }
}

TEST(TargetCapture, Bgra8IsReorderedToRgba) {
    const Bytes bytes{1, 2, 3, 4};

    for(const auto format:
        {RHIPixelFormat::BGRA8_UNORM, RHIPixelFormat::BGRA8_UNORM_SRGB}) {
        EXPECT_EQ(Convert(format, bytes, 1, 1, 4), (Bytes{3, 2, 1, 4}));
    }
}

TEST(TargetCapture, HalfFloatIsClampedAndRounded) {
    // 0.25, 0.5, 1.5, -0.25
    const auto bytes = BytesOf<u16>({0x3400, 0x3800, 0x3E00, 0xB400});

    EXPECT_EQ(
        Convert(RHIPixelFormat::RGBA16_FLOAT, bytes, 1, 1, 8),
        (Bytes{64, 128, 255, 0})
    );
}

TEST(TargetCapture, HalfFloatSpecialValues) {
    EXPECT_EQ(halfToFloat(0x3C00), 1.0f);
    EXPECT_EQ(halfToFloat(0x7BFF), 65504.0f);
    EXPECT_EQ(halfToFloat(0x0001), 0x1p-24f);
    EXPECT_EQ(halfToFloat(0x8000), -0.0f);

    // +inf, NaN, -inf, the smallest subnormal
    const auto bytes = BytesOf<u16>({0x7C00, 0x7E00, 0xFC00, 0x0001});

    EXPECT_EQ(
        Convert(RHIPixelFormat::RGBA16_FLOAT, bytes, 1, 1, 8),
        (Bytes{255, 0, 0, 0})
    );
}

TEST(TargetCapture, DepthBecomesGreyThroughTheDepthViewCurve) {
    const auto bytes = BytesOf<f32>({0.0f, 1.0f, 0.99f});

    EXPECT_EQ(
        Convert(RHIPixelFormat::D32_FLOAT, bytes, 3, 1, 12),
        (Bytes{255, 255, 255, 255, 0, 0, 0, 255, 162, 162, 162, 255})
    );
}

TEST(TargetCapture, AFormatCaptureCannotConvertIsRefused) {
    for(const auto format:
        {RHIPixelFormat::D24_UNORM_S8_UINT,
         RHIPixelFormat::D32_FLOAT_S8_UINT,
         RHIPixelFormat::RGBA32_FLOAT,
         RHIPixelFormat::R8_UNORM,
         RHIPixelFormat::BC1_UNORM}) {
        EXPECT_FALSE(isCapturable(format));

        Bytes rgba;
        EXPECT_FALSE(convertCapture(format, Bytes(64), 1, 1, 64, rgba));
    }
    EXPECT_TRUE(isCapturable(RHIPixelFormat::RGBA16_FLOAT));
    EXPECT_TRUE(isCapturable(RHIPixelFormat::D32_FLOAT));
}

TEST(TargetCaptureQueue, ATakenTargetOrPathIsQueued) {
    // a directory that does not exist, so the write fails and leaves nothing
    constexpr StrView Path = "no-such-directory/a.bmp";

    FakeDevice device;
    TargetCaptureQueue queue(device);
    queue.Request({.frame = 10, .target = "SceneColor", .path = Str{Path}});

    EXPECT_TRUE(queue.IsQueued(10, "SceneColor", "b.bmp"));
    EXPECT_TRUE(queue.IsQueued(11, "SceneDepth", Path));
    EXPECT_FALSE(queue.IsQueued(10, "SceneDepth", "b.bmp"));
    EXPECT_FALSE(queue.IsQueued(11, "SceneColor", "b.bmp"));
    // no target: the back buffer's form asks about the path alone
    EXPECT_TRUE(queue.IsQueued(10, {}, Path));
    EXPECT_FALSE(queue.IsQueued(10, {}, "b.bmp"));

    // a copy in flight keeps both
    const auto due = queue.TakeDue(10);
    ASSERT_EQ(due.size(), 1u);
    queue.AddInFlight(Readback(10, 10, due[0].path));
    EXPECT_TRUE(queue.IsQueued(10, "SceneColor", "b.bmp"));
    EXPECT_TRUE(queue.IsQueued(12, "SceneDepth", Path));

    // an outcome not yet taken keeps the path
    queue.Collect(10);
    EXPECT_FALSE(queue.IsQueued(10, "SceneColor", "b.bmp"));
    EXPECT_TRUE(queue.IsQueued(12, "SceneDepth", Path));
    EXPECT_EQ(queue.Pending(), 1u);

    const auto outcomes = queue.TakeOutcomes();
    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_FALSE(outcomes[0].written);
    EXPECT_FALSE(queue.IsQueued(12, "SceneDepth", Path));
    EXPECT_EQ(queue.Pending(), 0u);
}

TEST(TargetCaptureQueue, DueRequestsAreTakenInFrameOrder) {
    FakeDevice device;
    TargetCaptureQueue queue(device);
    queue.Request({.frame = 20, .target = "SceneDepth", .path = "b.bmp"});
    queue.Request({.frame = 10, .target = "SceneColor", .path = "a.bmp"});
    queue.Request({.frame = 10, .target = "SceneDepth", .path = "c.bmp"});
    EXPECT_EQ(queue.Pending(), 3u);

    EXPECT_TRUE(queue.TakeDue(9).empty());

    const auto first = queue.TakeDue(10);
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0].path, "a.bmp");
    EXPECT_EQ(first[1].path, "c.bmp");

    // a frame that recorded nothing passes its requests on
    const auto second = queue.TakeDue(25);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(second[0].path, "b.bmp");
    EXPECT_EQ(queue.Pending(), 0u);

    queue.Fail(second[0], "the pipeline has no such target");
    EXPECT_EQ(queue.Pending(), 1u);
    const auto outcomes = queue.TakeOutcomes();
    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_EQ(outcomes[0].requested, 20u);
    EXPECT_EQ(outcomes[0].path, "b.bmp");
    EXPECT_FALSE(outcomes[0].written);
    EXPECT_EQ(queue.Pending(), 0u);
}

TEST(TargetCaptureQueue, CollectWritesOnlyCompletedFramesAndRetires) {
    FakeDevice device;
    TargetCaptureQueue queue(device);
    const auto path =
        (std::filesystem::temp_directory_path() / "CrowyTargetCapture.bmp")
            .string();
    std::filesystem::remove(path);

    // asked for frame 5, recorded in 6: a frame that recorded nothing
    queue.AddInFlight(Readback(5, 6, path));

    queue.Collect(5);
    EXPECT_TRUE(queue.TakeOutcomes().empty());
    EXPECT_EQ(queue.Pending(), 1u);
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_TRUE(device.deferred.empty());

    queue.Collect(6);
    const auto outcomes = queue.TakeOutcomes();
    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_EQ(outcomes[0].requested, 5u);
    EXPECT_EQ(outcomes[0].presented, 6u);
    EXPECT_EQ(outcomes[0].path, path);
    EXPECT_TRUE(outcomes[0].written);
    EXPECT_EQ(queue.Pending(), 0u);
    // a 54-byte header and two 32-bit pixels
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(std::filesystem::file_size(path), 54u + 2 * 4);
    EXPECT_EQ(device.deferred.size(), 1u);

    std::filesystem::remove(path);
}
