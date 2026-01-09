add_executable(CrowyRHITest)
crowy_link_folder(CrowyRHITest asset)

file(GLOB ALL_SOURCES
    "*.hpp"
    "*.cpp"
)

target_sources(CrowyRHITest
PRIVATE
    ${ALL_SOURCES}
)

target_link_libraries(CrowyRHITest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    Crowy::Log
    Crowy::RHI
)
if(APPLE)
    target_link_libraries(CrowyRHITest
    PRIVATE
        Crowy::MetalRHI
    )
else()
    target_link_libraries(CrowyRHITest
    PRIVATE
        Crowy::NullRHI
    )
endif()

include(GoogleTest)
add_test(
    NAME CrowyRHI-Test
    COMMAND $<TARGET_FILE:CrowyRHITest>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/asset/Shaders/
)
set_tests_properties(CrowyRHI-Test PROPERTIES LABELS "unit;engine")