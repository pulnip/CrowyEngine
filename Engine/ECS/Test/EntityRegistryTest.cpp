#include <gtest/gtest.h>
#include "EntityRegistry.hpp"

using namespace Crowy;

TEST(ArchetypeView, SimpleQuery){
    EntityRegistry registry;
    Vec4 testColors[] = {
        {0.1, 0.2, 0.3, 0.5},
        {0.3, 0.7, 0.2, 0.1},
        {0.6, 0.9, 0.1, 0.2}
    };

    for(size_t i=0; i<3; ++i){
        registry.createEntity(
            ColorComponent{
                .color = testColors[i]
            }
        );
    }

    size_t i=0;
    for(auto [id, bit, cc]: registry.query<ColorComponent>()){
        EXPECT_EQ(cc.color, testColors[i]);
        ++i;
    }
}

TEST(ArchetypeView, ComplexQuery){
    EntityRegistry registry;
    Vec4 testColors[] = {
        {0.1, 0.2, 0.3, 0.5},
        {0.4, 0.3, 0.9, 1.0},
        {0.5, 0.1, 0.0, 0.5}
    };

    for(size_t i=0; i<3; ++i){
        registry.createEntity(
            TransformComponent{
                .position = zeros(),
                .rotation = unit_quat(),
                .scale = ones()
            },
            ColorComponent{
                .color = testColors[i]
            }
        );
    }

    size_t i=0;
    for(auto [id, bit, tc, cc]: registry.query<TransformComponent, ColorComponent>()){
        EXPECT_EQ(tc.position,    zeros());
        EXPECT_EQ(tc.rotation, unit_quat());
        EXPECT_EQ(   tc.scale,     ones());

        EXPECT_EQ(cc.color, testColors[i]);
        ++i;
    }
}

TEST(ArchetypeView, EmplaceOrder){
    EntityRegistry registry;
    Vec4 testColors[] = {
        {0.1, 0.2, 0.3, 0.5},
        {0.4, 0.3, 0.9, 1.0},
        {0.5, 0.1, 0.0, 0.5}
    };

    for(size_t i=0; i<3; ++i){
        if(i % 2 == 1){
            registry.createEntity(
                TransformComponent{
                    .position = zeros(),
                    .rotation = unit_quat(),
                    .scale = ones()
                },
                ColorComponent{
                    .color = testColors[i]
                }
            );
        }
        else{
            registry.createEntity(
                ColorComponent{
                    .color = testColors[i]
                },
                TransformComponent{
                    .position = zeros(),
                    .rotation = unit_quat(),
                    .scale = ones()
                }
            );
        }
    }

    size_t i=0;
    for(auto [id, bit, tc, cc]: registry.query<TransformComponent, ColorComponent>()){
        EXPECT_EQ(tc.position,    zeros());
        EXPECT_EQ(tc.rotation, unit_quat());
        EXPECT_EQ(   tc.scale,     ones());

        EXPECT_EQ(cc.color, testColors[i]);
        ++i;
    }
}

TEST(ArchetypeView, AppendComponent){
    EntityRegistry registry;
    Vec4 testColors[] = {
        {0.1, 0.2, 0.3, 0.5},
        {0.4, 0.3, 0.9, 1.0},
        {0.5, 0.1, 0.0, 0.5}
    };
    auto colorTest = [&testColors](Vec4 color){
        for(size_t i=0; i<3; ++i){
            if(testColors[i] == color)
                return i;
        }
        return size_t(10000);
    };
    EntityID entities[3];

    for(size_t i=0; i<3; ++i){
        entities[i] = registry.createEntity(
            ColorComponent{
                .color = testColors[i]
            },
            TransformComponent{
                .position = zeros(),
                .rotation = unit_quat(),
                .scale = ones()
            }
        );
    }

    auto testVal = 0;
    auto count = 0;
    for(auto [id, bit, tc, cc]: registry.query<TransformComponent, ColorComponent>()){
        EXPECT_EQ(tc.position,    zeros());
        EXPECT_EQ(tc.rotation, unit_quat());
        EXPECT_EQ(   tc.scale,     ones());

        // cannot predict query order.
        testVal += colorTest(cc.color);
        ++count;
    }
    EXPECT_EQ(testVal, 0+1+2);
    EXPECT_EQ(count, 3);
}

TEST(ArchetypeView, RemoveComponent){
    EntityRegistry registry;
    Vec4 testColors[] = {
        {0.1, 0.2, 0.3, 0.5},
        {0.4, 0.3, 0.9, 1.0},
        {0.5, 0.1, 0.0, 0.5}
    };
    auto colorTest = [&testColors](Vec4 color){
        for(size_t i=0; i<3; ++i){
            if(testColors[i] == color)
                return i;
        }
        return size_t(10000);
    };
    EntityID entities[3];

    for(size_t i=0; i<3; ++i){
        entities[i] = registry.createEntity(
            ColorComponent{
                .color = testColors[i]
            },
            TransformComponent{
                .position = zeros(),
                .rotation = unit_quat(),
                .scale = ones()
            }
        );
    }

    registry.removeComponent<ColorComponent>(entities[1]);

    auto testVal = 0;
    auto count = 0;
    for(auto [id, bit, tc, cc]: registry.query<TransformComponent, ColorComponent>()){
        EXPECT_EQ(tc.position,    zeros());
        EXPECT_EQ(tc.rotation, unit_quat());
        EXPECT_EQ(   tc.scale,     ones());

        // cannot predict query order.
        testVal += colorTest(cc.color);
        ++count;
    }
    EXPECT_EQ(testVal, 0+2);
    EXPECT_EQ(count, 2);

    count = 0;
    for(auto [id, bit, tc]: registry.query<TransformComponent>()){
        EXPECT_EQ(tc.position,    zeros());
        EXPECT_EQ(tc.rotation, unit_quat());
        EXPECT_EQ(   tc.scale,     ones());

        ++count;
    }
    EXPECT_EQ(count, 3);
}