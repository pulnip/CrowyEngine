#include <algorithm>
#include <format>
#include <random>
#include <unordered_set>
#include <vector>
#include <gtest/gtest.h>
#include "PairedHierarchy.hpp"
#include "Primitives.hpp"

using namespace Crowy;

namespace{
    constexpr usize NODE_BUDGET = 300;
    constexpr usize DUMP_TAIL = 40;

    class FuzzRunner{
    private:
        u32 seed;
        std::mt19937 rng;
        PairedHierarchy paired;
        std::vector<Str> log;
        // A pending reparent pins both ends: destroying either before the commit
        // is not allowed
        std::unordered_set<usize> pinned;

    public:
        explicit FuzzRunner(u32 seed)
            : seed(seed)
            , rng(seed){}

        void Run(usize operations){
            for(usize i=0; i<operations; ++i){
                step();

                if(::testing::Test::HasFailure()){
                    dump(i);

                    return;
                }
            }

            paired.ExpectAgreement();
            if(::testing::Test::HasFailure()){
                dump(operations);
            }
        }

        usize LivingCount() const noexcept{
            return paired.LivingCount();
        }

    private:
        usize roll(usize bound){
            return std::uniform_int_distribution<usize>{0, bound - 1}(rng);
        }

        Transform randomLocal(){
            auto axis = std::uniform_real_distribution<f32>{-2.0f, 2.0f};
            auto scale = std::uniform_real_distribution<f32>{0.5f, 1.5f};
            auto uniform = scale(rng);

            return Transform{
                .position = Vec3{axis(rng), axis(rng), axis(rng)},
                .scale = Vec3{uniform, uniform, uniform}
            };
        }

        void step(){
            auto living = paired.LivingIndexes();
            if(living.empty()){
                create(living);

                return;
            }

            auto weight = roll(100);
            if(living.size() > NODE_BUDGET){
                // over budget, so lean on destroy until it comes back down
                weight = weight < 60 ? 100 : weight;
            }

            if(weight < 35){
                create(living);
            }
            else if(weight < 60){
                setLocal(living);
            }
            else if(weight < 75){
                setParent(living);
            }
            else if(weight < 85){
                destroy(living);
            }
            else if(weight < 90){
                destroySubtree(living);
            }
            else{
                commit();
            }
        }

        void create(const std::vector<usize>& living){
            auto parent = PairedHierarchy::NO_PARENT;
            // a quarter of them start as roots, so the forest keeps several trees
            if(!living.empty() && roll(4) != 0){
                parent = living[roll(living.size())];
            }

            auto index = paired.Create(parent, randomLocal());
            log.push_back(std::format("create(parent={})", static_cast<i64>(parent)));
            (void)index;
        }

        void setLocal(const std::vector<usize>& living){
            auto index = living[roll(living.size())];

            paired.SetLocal(index, randomLocal());
            log.push_back(std::format("setLocal({})", index));
        }

        void setParent(const std::vector<usize>& living){
            auto index = living[roll(living.size())];

            auto parent = PairedHierarchy::NO_PARENT;
            if(roll(4) != 0){
                auto candidate = living[roll(living.size())];
                // a node cannot move under its own descendant, or under itself
                if(paired.IsInSubtreeOf(index, candidate)){
                    return;
                }
                parent = candidate;
            }

            // the old parent loses a child in the reference right away, while the
            // real one keeps it until the commit, so it cannot be destroyed either
            auto oldParent = paired.ParentOf(index);

            paired.SetParent(index, parent);
            log.push_back(std::format("setParent({}, {})", index, static_cast<i64>(parent)));

            pinned.insert(index);
            if(parent != PairedHierarchy::NO_PARENT){
                pinned.insert(parent);
            }
            if(oldParent != PairedHierarchy::NO_PARENT){
                pinned.insert(oldParent);
            }
        }

        void destroy(const std::vector<usize>& living){
            // a leaf that no pending command is holding on to
            for(usize attempt=0; attempt<8; ++attempt){
                auto index = living[roll(living.size())];
                if(!paired.IsLeaf(index) || pinned.contains(index)){
                    continue;
                }

                paired.Destroy(index);
                log.push_back(std::format("destroy({})", index));

                return;
            }
        }

        void destroySubtree(const std::vector<usize>& living){
            for(usize attempt=0; attempt<8; ++attempt){
                auto index = living[roll(living.size())];
                auto subtree = paired.SubtreeOf(index);

                auto held = std::ranges::any_of(subtree, [this](usize member){
                    return pinned.contains(member);
                });
                if(held){
                    continue;
                }

                paired.DestroySubtree(index);
                log.push_back(std::format(
                    "destroySubtree({}) of {}", index, subtree.size()
                ));

                return;
            }
        }

        void commit(){
            paired.ExpectAgreement();
            log.push_back("commit");
            pinned.clear();
        }

        void dump(usize operation) const{
            Str tail;
            auto from = log.size() > DUMP_TAIL ? log.size() - DUMP_TAIL : 0;
            for(auto i=from; i<log.size(); ++i){
                tail += std::format("  [{}] {}\n", i, log[i]);
            }

            ADD_FAILURE() << std::format(
                "fuzz failed at operation {} with seed {}\nlast {} operations:\n{}",
                operation, seed, log.size() - from, tail
            );
        }
    };
}

class TransformFuzzTest: public ::testing::TestWithParam<u32>{};

TEST_P(TransformFuzzTest, SeededRun){
    FuzzRunner runner{GetParam()};

    runner.Run(3000);
}

INSTANTIATE_TEST_SUITE_P(
    Seeds,
    TransformFuzzTest,
    ::testing::Values(1u, 7u, 42u, 1337u, 20260815u)
);

// Minutes per seed, so it is opt in:
//   CrowySceneTest --gtest_also_run_disabled_tests --gtest_filter=*FuzzLong*
class TransformFuzzLongTest: public ::testing::TestWithParam<u32>{};

TEST_P(TransformFuzzLongTest, DISABLED_SeededRun){
    FuzzRunner runner{GetParam()};

    runner.Run(100000);
}

INSTANTIATE_TEST_SUITE_P(
    Seeds,
    TransformFuzzLongTest,
    ::testing::Values(977u, 1954u, 2931u, 3908u, 4885u, 5862u, 6839u, 7816u, 8793u, 9770u)
);
