#include <gtest/gtest.h>
#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "Semantics.hpp"
#include "TomlLoader.hpp"

using namespace Crowy;

class CreationTestObject final: public Object{
    CROWY_OBJECT_BODY(CreationTestObject)

public:
    CreationTestObject() = default;
    ~CreationTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(CreationTestObject)
};

CROWY_OBJECT(CreationTestObject)
CROWY_OBJECT_END(CreationTestObject)

TEST(Reflection, CreationTestObject){
    ASSERT_TRUE(IsCreationTestObjectRegistered);

    CStr objectName = "CreationTestObject";
    auto object = ClassRegistry::Create(objectName);
    ASSERT_TRUE(object != nullptr);
    EXPECT_TRUE(object->GetClassName() == objectName);
    EXPECT_TRUE(object->IsA("Object"));
    EXPECT_TRUE(object->IsA(objectName));

    auto testObject = dynamic_cast<CreationTestObject*>(object.get());
    EXPECT_TRUE(testObject != nullptr);
}

class InjectionTestObject final: public Object{
    CROWY_OBJECT_BODY(InjectionTestObject)

public:
    InjectionTestObject() = default;
    ~InjectionTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(InjectionTestObject)

    Transform transform;
};

CROWY_OBJECT(InjectionTestObject)
    .SetProperty("transform", &InjectionTestObject::transform)
    .SetProperty("position", &InjectionTestObject::transform, &Transform::position)
    .SetProperty("rotation", &InjectionTestObject::transform, &Transform::rotation)
    .SetProperty("scale", &InjectionTestObject::transform, &Transform::scale)
CROWY_OBJECT_END(InjectionTestObject)

TEST(Reflection, NestedProperty){
    ASSERT_TRUE(IsInjectionTestObjectRegistered);

    auto object = Crowy::ClassRegistry::Create("InjectionTestObject");
    ASSERT_TRUE(object != nullptr);

    auto nested = dynamic_cast<InjectionTestObject*>(object.get());
    ASSERT_TRUE(nested != nullptr);



    {
        auto dom = Crowy::parseTomlString(R"(
[transform]
position = [1.0, 2.0, 3.0]
rotation = [0.0, 0.0, 0.0, 1.0]
scale = [4.0, 5.0, 6.0]
)");
        Crowy::ApplyProperties(nested, dom);

        Transform expected{
            .position = Vec3{1.0, 2.0, 3.0},
            .rotation = Vec4{0.0, 0.0, 0.0, 1.0},
            .scale = Vec3{4.0, 5.0, 6.0},
        };

        EXPECT_EQ(nested->transform.position, expected.position);
        EXPECT_EQ(nested->transform.rotation, expected.rotation);
        EXPECT_EQ(nested->transform.scale, expected.scale);
    }

    {
        auto dom = Crowy::parseTomlString(R"(
position = [3.0, 2.0, 1.0]
rotation = [1.0, 0.0, 0.0, 0.0]
scale = [6.0, 5.0, 4.0]
)");
        Crowy::ApplyProperties(nested, dom);

        Transform expected{
            .position = Vec3{3.0, 2.0, 1.0},
            .rotation = Vec4{1.0, 0.0, 0.0, 0.0},
            .scale = Vec3{6.0, 5.0, 4.0},
        };

        EXPECT_EQ(nested->transform.position, expected.position);
        EXPECT_EQ(nested->transform.rotation, expected.rotation);
        EXPECT_EQ(nested->transform.scale, expected.scale);
    }
}
