# Backlot's film set, read from its scene file and placed into a RenderScene.
# Everything that knows Backlot's contract lives here, outside the engine.
add_library(SampleStageArchive STATIC
    StageDocument.cpp
    StageInspect.cpp
    StageLoad.cpp
    StageScene.cpp
)

target_include_directories(SampleStageArchive
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SampleStageArchive
PUBLIC
    Crowy::Core
    Crowy::Reflection
    Crowy::Render
    Crowy::Serialization
)

# StageInspect only registers the rows
crowy_whole_archive(SampleStage SampleStageArchive)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SampleStage
        DIRECTORY Test
        DEPENDS SampleStage
        LABELS "unit"
    )

    # the real film set, read in place; without it the binary exits 77
    # before any test runs, which ctest reports as skipped
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
        SKIP_RETURN_CODE 77
    )
endif()
