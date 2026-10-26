#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "AssetStore.hpp"

using namespace Crowy;

namespace
{
    using Paths = std::vector<Str>;

    // move-only, as a GPU asset is
    struct Blob {
        Str path;
        RAII<i32> owned;
    };

    using BlobStore = AssetStore<Blob>;
    using BlobHandle = AssetHandle<Blob>;

    AssetRef parsed(StrView text) {
        return AssetRef::Parse(text).value();
    }

    BlobHandle loaded(BlobStore& store, StrView text) {
        auto result = store.Load(parsed(text));
        EXPECT_TRUE(result.has_value()) << result.error_or("");
        return result.value_or(BlobHandle{});
    }

    class AssetStoreTest: public testing::Test {
    protected:
        Paths loads;
        Paths releases;
        bool failNextLoad = false;
        // held outside the store, dropped when the asset named by the key is
        // released
        std::optional<std::pair<Str, BlobHandle>> heldUntilRelease;
        BlobStore store{
            [this](const AssetRef& ref) {
                if(std::exchange(failNextLoad, false))
                    return BlobStore::Loaded(std::unexpect, "no such file");

                loads.push_back(ref.ToString());
                return BlobStore::Loaded(Blob{.path = ref.ToString()});
            },
            [this](Blob blob) {
                releases.push_back(blob.path);
                if(heldUntilRelease && heldUntilRelease->first == blob.path)
                    heldUntilRelease.reset();
            }
        };
    };
}

TEST_F(AssetStoreTest, APathLoadsOnceWhateverItsSpelling) {
    const auto first = loaded(store, "a.png");
    const auto second = loaded(store, "./b/../a.png");

    EXPECT_EQ(loads, Paths{"a.png"});
    EXPECT_EQ(&*first, &*second);
    EXPECT_EQ(second.Ref(), parsed("a.png"));
    EXPECT_EQ(store.Count(), 1u);
}

TEST_F(AssetStoreTest, TheLastHolderOnlyMarksUntilCollect) {
    {
        const auto handle = loaded(store, "a.png");
        const auto copy = handle;
    }
    EXPECT_TRUE(releases.empty());
    EXPECT_EQ(store.Count(), 1u);

    store.Collect();
    EXPECT_EQ(releases, Paths{"a.png"});
    EXPECT_EQ(store.Count(), 0u);

    store.Collect();
    EXPECT_EQ(releases, Paths{"a.png"});
}

TEST_F(AssetStoreTest, AHolderBeforeCollectKeepsTheAsset) {
    loaded(store, "a.png");
    const auto again = loaded(store, "a.png");

    store.Collect();
    EXPECT_EQ(loads, Paths{"a.png"});
    EXPECT_TRUE(releases.empty());
    EXPECT_EQ(again->path, "a.png");
}

TEST_F(AssetStoreTest, AReleasedPathLoadsAgain) {
    loaded(store, "a.png");
    store.Collect();

    const auto again = loaded(store, "a.png");
    EXPECT_EQ(loads, (Paths{"a.png", "a.png"}));
    EXPECT_EQ(store.Count(), 1u);
}

TEST_F(AssetStoreTest, AFailedLoadLeavesNothingBehind) {
    failNextLoad = true;
    const auto failed = store.Load(parsed("./a.png"));
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), "a.png: no such file");
    EXPECT_EQ(store.Count(), 0u);

    const auto retried = loaded(store, "a.png");
    EXPECT_EQ(loads, Paths{"a.png"});
    EXPECT_EQ(retried->path, "a.png");
}

TEST_F(AssetStoreTest, CopiesAndMovesKeepTheCount) {
    auto handle = loaded(store, "a.png");
    auto copy = handle;
    auto moved = std::move(handle);
    EXPECT_FALSE(handle);
    EXPECT_TRUE(moved);

    auto& alias = copy;
    copy = alias;
    moved = BlobHandle{};
    store.Collect();
    EXPECT_TRUE(releases.empty());

    copy = std::move(moved);
    store.Collect();
    EXPECT_EQ(releases, Paths{"a.png"});
}

TEST_F(AssetStoreTest, ACollectReleasesWhatAReleaseLeavesUnheld) {
    heldUntilRelease.emplace("a.png", loaded(store, "b.png"));
    loaded(store, "a.png");

    store.Collect();
    EXPECT_EQ(releases, (Paths{"a.png", "b.png"}));
    EXPECT_EQ(store.Count(), 0u);
}

TEST(AssetStore, ADestroyedStoreReleasesWhatNoHandleHolds) {
    Paths releases;
    {
        BlobStore store{
            [](const AssetRef& ref) { return Blob{.path = ref.ToString()}; },
            [&](Blob blob) { releases.push_back(blob.path); }
        };
        loaded(store, "a.png");
    }
    EXPECT_EQ(releases, Paths{"a.png"});
}

#if CROWY_DEBUG_BUILD
TEST(AssetStoreDeathTest, AHandleOutlivingItsStoreStops) {
    EXPECT_DEATH(
        {
            std::optional<BlobStore> store;
            store.emplace([](const AssetRef& ref) {
                return Blob{.path = ref.ToString()};
            });
            const auto handle = loaded(*store, "a.png");
            store.reset();
        },
        "an asset handle outlives its store"
    );
}
#endif
