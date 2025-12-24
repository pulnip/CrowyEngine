add_executable(CrowyResourceTest)

target_sources(CrowyResourceTest
PRIVATE
    ResourceModuleTest.cpp
)

target_link_libraries(CrowyResourceTest
PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    CrowyResource
)

if(APPLE)
    target_link_libraries(CrowyResourceTest
    PRIVATE
        Crowy::MetalRHI
    )
endif()

include(GoogleTest)
add_test(
    NAME CrowyResource-Test
    COMMAND $<TARGET_FILE:CrowyResourceTest>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(CrowyResource-Test PROPERTIES LABELS "unit;engine")