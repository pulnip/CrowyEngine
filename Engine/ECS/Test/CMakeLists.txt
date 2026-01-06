add_executable(CrowyECSTest)

file(GLOB ALL_SOURCES
    "*.hpp"
    "*.cpp"
)

target_sources(CrowyECSTest
PRIVATE
    ${ALL_SOURCES}
)

target_link_libraries(CrowyECSTest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    Crowy::ECS
)

include(GoogleTest)
add_test(
    NAME CrowyECS-Test
    COMMAND $<TARGET_FILE:CrowyECSTest>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(CrowyECS-Test PROPERTIES LABELS "unit;engine")