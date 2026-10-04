#include <cmath>

#include <gtest/gtest.h>

#include "Puzzle.hpp"

using namespace Crowy;

namespace
{
    const auto Start = BodyPose{.position = {0.0f, 1.0f, 0.0f}};

    PoseTrack slideTo(f32 x, u64 tick, Easing easing) {
        return PoseTrack{
            .keys = {
                PoseKey{
                    .tick = tick,
                    .pose = BodyPose{.position = {x, 1.0f, 0.0f}},
                    .easing = easing,
                },
            },
        };
    }

    Vec4 turnAboutY(f32 halfAngle) {
        return Vec4{0.0f, std::sin(halfAngle), 0.0f, std::cos(halfAngle)};
    }
}

TEST(PuzzleTrack, StartsAtTheDescPose) {
    const auto track = slideTo(1.0f, 4, Easing::Linear);
    const auto pose = poseAt(track, Start, 0);
    EXPECT_EQ(pose.position, Start.position);
    EXPECT_EQ(pose.rotation, Start.rotation);
}

TEST(PuzzleTrack, KeysAreExactAndTheLastHolds) {
    auto track = slideTo(1.0f, 10, Easing::Smooth);
    const auto turned = turnAboutY(0.3f);
    track.keys.push_back(
        PoseKey{
            .tick = 20,
            .pose = BodyPose{
                .position = {3.0f, 2.0f, -1.0f},
                .rotation = turned,
            },
        }
    );
    EXPECT_EQ(poseAt(track, Start, 10).position, track.keys[0].pose.position);
    EXPECT_EQ(poseAt(track, Start, 10).rotation, track.keys[0].pose.rotation);
    for(const u64 tick: {20u, 21u, 600u}) {
        SCOPED_TRACE(tick);
        const auto pose = poseAt(track, Start, tick);
        EXPECT_EQ(pose.position, track.keys[1].pose.position);
        EXPECT_EQ(pose.rotation, turned);
    }
}

TEST(PuzzleTrack, EachSegmentStartsAtTheKeyBefore) {
    auto track = slideTo(1.0f, 10, Easing::Linear);
    track.keys.push_back(
        PoseKey{
            .tick = 20,
            .pose = BodyPose{.position = {3.0f, 1.0f, 0.0f}},
            .easing = Easing::Linear,
        }
    );
    EXPECT_EQ(poseAt(track, Start, 5).position.x, 0.5f);
    EXPECT_EQ(poseAt(track, Start, 15).position.x, 2.0f);
}

TEST(PuzzleTrack, EasingShapesTheQuarter) {
    struct Case {
        Easing easing;
        f32 x;
    };
    // a quarter of the way through the time
    for(const auto [easing, x]: {
            Case{Easing::Linear, 0.25f},
            Case{Easing::Smooth, 0.15625f},
            Case{Easing::EaseIn, 0.0625f},
            Case{Easing::EaseOut, 0.4375f},
        }) {
        SCOPED_TRACE(enumName(easing));
        const auto pose = poseAt(slideTo(1.0f, 4, easing), Start, 1);
        EXPECT_EQ(pose.position.x, x);
        EXPECT_EQ(pose.position.y, 1.0f);
    }
}

TEST(PuzzleTrack, TurnsHalfwayAtHalfTime) {
    const auto quarterTurn = turnAboutY(std::acos(-1.0f) / 4.0f);
    const auto track = PoseTrack{
        .keys = {
            PoseKey{
                .tick = 8,
                .pose = BodyPose{
                    .position = Start.position,
                    .rotation = quarterTurn,
                },
                .easing = Easing::Linear,
            },
        },
    };
    const auto rotation = poseAt(track, Start, 4).rotation;
    const auto expected = turnAboutY(std::acos(-1.0f) / 8.0f);
    EXPECT_NEAR(rotation.x, expected.x, 1.0e-6f);
    EXPECT_NEAR(rotation.y, expected.y, 1.0e-6f);
    EXPECT_NEAR(rotation.z, expected.z, 1.0e-6f);
    EXPECT_NEAR(rotation.w, expected.w, 1.0e-6f);
}

TEST(PuzzleTrack, HoldsATurnedPoseExactly) {
    const auto turned = BodyPose{
        .position = {0.3f, 1.2f, -0.4f},
        .rotation = turnAboutY(0.7f),
    };
    const auto track = PoseTrack{
        .keys = {
            PoseKey{.tick = 10, .pose = turned},
            PoseKey{.tick = 40, .pose = turned},
        },
    };
    for(u64 tick = 10; tick <= 40; ++tick) {
        SCOPED_TRACE(tick);
        const auto pose = poseAt(track, Start, tick);
        EXPECT_EQ(pose.position, turned.position);
        EXPECT_EQ(pose.rotation, turned.rotation);
    }
}

TEST(PuzzleTrack, TakesTheShorterWay) {
    // the negated identity is the same orientation: nothing turns
    const auto track = PoseTrack{
        .keys = {
            PoseKey{
                .tick = 8,
                .pose = BodyPose{
                    .position = Start.position,
                    .rotation = {0.0f, 0.0f, 0.0f, -1.0f},
                },
                .easing = Easing::Linear,
            },
        },
    };
    EXPECT_EQ(poseAt(track, Start, 4).rotation, unitQuat());
}

TEST(PuzzleDeathTest, TrackKeysMustRise) {
    auto track = slideTo(1.0f, 10, Easing::Linear);
    track.keys.push_back(track.keys[0]);
    EXPECT_DEATH(poseAt(track, Start, 15), "a track's keys rise");
}
