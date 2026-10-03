# Backlot's film set, read from its scene file and placed into a RenderScene.
# Everything that knows Backlot's contract lives here, outside the engine.
add_library(SampleStage STATIC
    StageDocument.cpp
)

target_include_directories(SampleStage
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SampleStage
PUBLIC
    Crowy::Core
    Crowy::Render
    Crowy::Serialization
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SampleStage
        DIRECTORY Test
        DEPENDS SampleStage
        LABELS "unit"
    )

    # the real film set, read in place; every test skips when it is missing,
    # and ctest then reports the entry as skipped rather than passed
    crowy_declare_test(SampleStageBacklot
        DIRECTORY BacklotTest
        DEPENDS SampleStage
        LABELS "unit;stage"
    )
    target_compile_definitions(CrowySampleStageBacklotTest
    PRIVATE
        CROWY_BACKLOT_DIR="${CROWY_BACKLOT_DIR}"
        CROWY_ENGINE_PROBE="${CMAKE_SOURCE_DIR}/Engine/Serialization/Test/Data/AxisProbe.fbx"
    )
    set_tests_properties(CrowySampleStageBacklotTest
    PROPERTIES
        SKIP_REGULAR_EXPRESSION "  SKIPPED ]"
    )
endif()
