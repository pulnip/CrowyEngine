#include <vector>

#include <gtest/gtest.h>

#include "AssetRef.hpp"
#include "AssetRoots.hpp"
#include "ClassRegistry.hpp"
#include "JsonLoader.hpp"

using namespace Crowy;

namespace
{
    struct RefHolder {
        AssetRef mesh;
        std::vector<AssetRef> textures;
    };

    AssetRef parsed(StrView text) {
        const auto ref = AssetRef::Parse(text);
        EXPECT_TRUE(ref.has_value()) << text;
        return ref.value_or(AssetRef{});
    }
}

namespace Crowy
{
    // clang-format off
    CROWY_STRUCT(RefHolder)
        .AddProperty("mesh", &RefHolder::mesh)
        .AddProperty("textures", &RefHolder::textures)
    CROWY_STRUCT_END(RefHolder)
    // clang-format on
}

TEST(AssetRef, APrefixNamesTheEngineMount) {
    const auto content = parsed("Textures/a.png");
    EXPECT_EQ(content.Mount(), AssetMount::Content);
    EXPECT_EQ(content.Path(), "Textures/a.png");
    EXPECT_EQ(content.ToString(), "Textures/a.png");

    const auto engine = parsed("engine:Builtin/Box");
    EXPECT_EQ(engine.Mount(), AssetMount::Engine);
    EXPECT_EQ(engine.Path(), "Builtin/Box");
    EXPECT_EQ(engine.ToString(), "engine:Builtin/Box");
}

TEST(AssetRef, EverySpellingOfAPathIsOneRef) {
    EXPECT_EQ(parsed("./a.png"), parsed("a.png"));
    EXPECT_EQ(parsed("b/../a.png"), parsed("a.png"));
    EXPECT_EQ(parsed("a//b.png").Path(), "a/b.png");
    EXPECT_EQ(parsed("a/b/").Path(), "a/b");
    EXPECT_EQ(parsed("engine:./Shader/../Builtin/Box").Path(), "Builtin/Box");

    // the same path under another mount is another asset
    EXPECT_NE(parsed("Builtin/Box"), parsed("engine:Builtin/Box"));
}

TEST(AssetRef, APathOutsideItsMountIsRefused) {
    for(const StrView text: {
            "",
            ".",
            "a/..",
            "../a.png",
            "a/../../b.png",
            "/a.png",
            "C:/a.png",
            "content:a.png",
            "engine:",
            "engine:../a.png",
            "engine:a:b",
            "a\\b.png",
        }) {
        EXPECT_FALSE(AssetRef::Parse(text).has_value()) << text;
    }
}

TEST(AssetRef, APropertyReadsAndWritesItsText) {
    const RefHolder holder{
        .mesh = parsed("engine:Builtin/Box"),
        .textures = {parsed("a.png"), AssetRef{}},
    };

    DOM::Value value;
    SerializeProperties(&holder, value);
    EXPECT_EQ(
        emitJson(value),
        R"({"mesh":"engine:Builtin/Box","textures":["a.png",null]})"
    );

    RefHolder back;
    EXPECT_TRUE(ApplyProperties(&back, value).empty());
    EXPECT_EQ(back.mesh, holder.mesh);
    EXPECT_EQ(back.textures, holder.textures);

    // a file's spelling reads as the one spelling
    EXPECT_TRUE(
        ApplyProperties(&back, parseJsonString(R"({"mesh":"./b/../a.png"})"))
            .empty()
    );
    EXPECT_EQ(back.mesh, parsed("a.png"));
}

TEST(AssetRef, AValueThatNamesNoAssetIsRefused) {
    RefHolder holder{.mesh = parsed("a.png")};
    const auto errors = ApplyProperties(
        &holder,
        parseJsonString(R"({"mesh":"../a.png","textures":["b.png",7]})")
    );
    EXPECT_EQ(
        errors,
        (PropertyErrors{
            "'mesh' expects AssetRef",
            "'textures[1]' expects AssetRef"
        })
    );
    EXPECT_EQ(holder.mesh, parsed("a.png"));
}

TEST(AssetRef, EachMountResolvesUnderItsRoot) {
    const AssetRoots roots{.engine = "root/Engine", .content = "root/Content"};

    EXPECT_EQ(
        resolveAssetPath(roots, parsed("engine:Builtin/Box")),
        std::filesystem::path("root/Engine") / "Builtin" / "Box"
    );
    EXPECT_EQ(
        resolveAssetPath(roots, parsed("Textures/a.png")),
        std::filesystem::path("root/Content") / "Textures" / "a.png"
    );
}
