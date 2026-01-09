add_executable(CrowyInputTest)

file(GLOB ALL_SOURCES
    "*.hpp"
    "*.cpp"
)

target_sources(CrowyInputTest
PRIVATE
    ${ALL_SOURCES}
)

target_link_libraries(CrowyInputTest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    Crowy::Input
)

include(GoogleTest)
add_test(
    NAME CrowyInput-Test
    COMMAND $<TARGET_FILE:CrowyInputTest>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(CrowyInput-Test PROPERTIES LABELS "unit;engine")