add_executable(CrowySceneTest)
crowy_link_assets(CrowySceneTest)

target_sources(CrowySceneTest
PRIVATE
    SceneLoadTest.cpp
    SceneModuleTest.cpp
)

target_link_libraries(CrowySceneTest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    Crowy::NullRHI
    CrowyScene_Private
)

include(GoogleTest)
add_test(
    NAME CrowyScene-Test
    COMMAND $<TARGET_FILE:CrowySceneTest>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(CrowyScene-Test PROPERTIES LABELS "unit;engine")