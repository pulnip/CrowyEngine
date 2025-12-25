add_executable(CrowyResourceTest)
crowy_link_assets(CrowyResourceTest)

target_sources(CrowyResourceTest
PRIVATE
    ModelSerializerTest.cpp
    ResourceModuleTest.cpp
)

target_link_libraries(CrowyResourceTest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    Crowy::NullRHI
    CrowyResource_Private
)

include(GoogleTest)
add_test(
    NAME CrowyResource-Test
    COMMAND $<TARGET_FILE:CrowyResourceTest>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(CrowyResource-Test PROPERTIES LABELS "unit;engine")