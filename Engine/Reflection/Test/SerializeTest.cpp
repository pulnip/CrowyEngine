#include <gtest/gtest.h>
#include "ClassRegistry.hpp"
#include "JsonLoader.hpp"

using namespace Crowy;

// non-contiguous and negative in a signed underlying type,
// so a name round trip cannot pass by accident of index
enum class SerializeBlend: i16{
    Opaque = -1,
    Masked = 7,
    Additive = 300
};

namespace Crowy
{
    CROWY_ENUM_BEGIN(SerializeBlend)
        CROWY_ENUM_VALUE(Opaque)
        CROWY_ENUM_VALUE(Masked)
        CROWY_ENUM_VALUE(Additive)
    CROWY_ENUM_END()
}

namespace
{
    // the erased pair the port will drive, through the text the wire carries
    template<typename T>
    T roundTrip(const T& in){
        const auto* ops = GetTypeOps<T>();
        DOM::Value out;
        ops->serialize(&in, out);

        const auto dom = parseJsonString(emitJson(out));
        T back{};
        EXPECT_TRUE(ops->deserialize(&back, dom));
        return back;
    }
}

TEST(Serialize, ScalarsRoundTrip){
    EXPECT_EQ(roundTrip<bool>(true), true);
    EXPECT_EQ(roundTrip<i8>(-8), -8);
    EXPECT_EQ(roundTrip<i16>(-16), -16);
    EXPECT_EQ(roundTrip<i32>(-32), -32);
    EXPECT_EQ(roundTrip<i64>(-64), -64);
    EXPECT_EQ(roundTrip<u8>(8), 8);
    EXPECT_EQ(roundTrip<u16>(16), 16);
    EXPECT_EQ(roundTrip<u32>(32), 32u);
    EXPECT_EQ(roundTrip<u64>(64), 64u);
    EXPECT_EQ(roundTrip<f32>(0.25f), 0.25f);
    EXPECT_EQ(roundTrip<f64>(0.125), 0.125);
    EXPECT_EQ(roundTrip<Str>("title"), "title");
}

TEST(Serialize, CompoundsRoundTrip){
    EXPECT_EQ(roundTrip(Vec2(1.0f, 2.0f)), Vec2(1.0f, 2.0f));
    EXPECT_EQ(roundTrip(Vec3(1.0f, 2.0f, 3.0f)), Vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(roundTrip(Vec4(1.0f, 2.0f, 3.0f, 4.0f)), Vec4(1.0f, 2.0f, 3.0f, 4.0f));

    const auto size = roundTrip(Size2D{640, 480});
    EXPECT_EQ(size.x, 640u);
    EXPECT_EQ(size.y, 480u);

    const auto transform = roundTrip(Transform{
        .position = Vec3(1.0f, 2.0f, 3.0f),
        .rotation = Vec4(0.0f, 0.0f, 1.0f, 0.0f),
        .scale = Vec3(2.0f, 2.0f, 2.0f)
    });
    EXPECT_EQ(transform.position, Vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(transform.rotation, Vec4(0.0f, 0.0f, 1.0f, 0.0f));
    EXPECT_EQ(transform.scale, Vec3(2.0f, 2.0f, 2.0f));
}

TEST(Serialize, EnumRoundTripsByName){
    EXPECT_EQ(roundTrip(SerializeBlend::Opaque), SerializeBlend::Opaque);
    EXPECT_EQ(roundTrip(SerializeBlend::Additive), SerializeBlend::Additive);
}

TEST(Serialize, ShapesFollowTheParsingConvention){
    DOM::Value out;

    const auto v = Vec3(1.0f, 2.0f, 3.0f);
    GetTypeOps<Vec3>()->serialize(&v, out);
    ASSERT_TRUE(out.is_array());
    EXPECT_EQ(out.asArray()->size(), 3u);
    EXPECT_TRUE((*out.asArray())[0].is_float());

    const auto size = Size2D{640, 480};
    GetTypeOps<Size2D>()->serialize(&size, out);
    ASSERT_TRUE(out.is_array());
    EXPECT_EQ(out.asArray()->size(), 2u);
    EXPECT_TRUE((*out.asArray())[0].is_int());

    const auto transform = Transform{};
    GetTypeOps<Transform>()->serialize(&transform, out);
    ASSERT_TRUE(out.is_table());
    EXPECT_EQ(out.asTable()->size(), 3u);
    EXPECT_TRUE(out.at("position") != nullptr);
    EXPECT_TRUE(out.at("rotation") != nullptr);
    EXPECT_TRUE(out.at("scale") != nullptr);

    const auto blend = SerializeBlend::Masked;
    GetTypeOps<SerializeBlend>()->serialize(&blend, out);
    ASSERT_TRUE(out.is_string());
    EXPECT_EQ(*out.asString(), "Masked");

    // no enumerator, no name
    const auto unlisted = static_cast<SerializeBlend>(9);
    GetTypeOps<SerializeBlend>()->serialize(&unlisted, out);
    EXPECT_TRUE(out.is_none());
}

TEST(Serialize, DeserializeReportsWhetherItWrote){
    f32 real = 0.5f;
    EXPECT_TRUE(GetTypeOps<f32>()->deserialize(&real, DOM::Value(static_cast<i64>(1))));
    EXPECT_EQ(real, 1.0f);
    EXPECT_FALSE(GetTypeOps<f32>()->deserialize(&real, DOM::Value("text")));
    EXPECT_EQ(real, 1.0f);

    i32 whole = 3;
    EXPECT_FALSE(GetTypeOps<i32>()->deserialize(&whole, DOM::Value(1.0)));
    EXPECT_EQ(whole, 3);

    Vec3 v(1.0f, 2.0f, 3.0f);
    DOM::Array two;
    two.emplace_back(0.0);
    two.emplace_back(0.0);
    EXPECT_FALSE(GetTypeOps<Vec3>()->deserialize(&v, DOM::Value(std::move(two))));
    EXPECT_EQ(v, Vec3(1.0f, 2.0f, 3.0f));

    auto blend = SerializeBlend::Masked;
    EXPECT_FALSE(GetTypeOps<SerializeBlend>()->deserialize(&blend, DOM::Value("Unknown")));
    EXPECT_EQ(blend, SerializeBlend::Masked);
    EXPECT_FALSE(GetTypeOps<SerializeBlend>()->deserialize(&blend, DOM::Value()));
    EXPECT_EQ(blend, SerializeBlend::Masked);
    EXPECT_TRUE(GetTypeOps<SerializeBlend>()->deserialize(&blend, DOM::Value("Opaque")));
    EXPECT_EQ(blend, SerializeBlend::Opaque);
}

TEST(Serialize, EnumOpsCarryTheEnumeratorValue){
    const auto* ops = GetTypeOps<SerializeBlend>();
    ASSERT_TRUE(ops->enumLoad != nullptr);
    ASSERT_TRUE(ops->enumStore != nullptr);
    // no enum ops on a leaf
    EXPECT_TRUE(GetTypeOps<i32>()->enumLoad == nullptr);

    // negative in a signed underlying type reads back signed,
    // the same i64 enumerators() lists it as
    auto blend = SerializeBlend::Opaque;
    EXPECT_EQ(ops->enumLoad(&blend), -1);
    EXPECT_EQ(ops->enumerators()[0].value, -1);

    ops->enumStore(&blend, 300);
    EXPECT_EQ(blend, SerializeBlend::Additive);
}
