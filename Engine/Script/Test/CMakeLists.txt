add_executable(CrowyScriptTest)
crowy_link_folder(CrowyScriptTest asset)

file(GLOB ALL_SOURCES
    "*.hpp"
    "*.cpp"
)

target_sources(CrowyScriptTest
PRIVATE
    ${ALL_SOURCES}
)

target_link_libraries(CrowyScriptTest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    Crowy::Script
)

include(GoogleTest)
add_test(
    NAME CrowyScript-Test
    COMMAND $<TARGET_FILE:CrowyScriptTest>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(CrowyScript-Test PROPERTIES LABELS "unit;engine")