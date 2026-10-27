# Backlot's film set, read from its scene file and placed into a RenderScene.
# Everything that knows Backlot's contract lives here, outside the engine.
crowy_declare_sample_library(Stage
    # StageInspect only registers the rows
    WHOLE_ARCHIVE
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    StageDocument.cpp
    StageInspect.cpp
    StageLoad.cpp
    StageScene.cpp
PUBLIC
    Crowy::Content
    Crowy::Core
    Crowy::Reflection
    Crowy::Render
    Crowy::Serialization
)

if(CROWY_ENABLE_TEST)
    # the real film set, read in place; without it the binary exits 77
    # before any test runs, which ctest reports as skipped
    crowy_declare_test(SampleStageBacklot
        DIRECTORY BacklotTest
        DEPENDS Crowy::SampleStage
        LABELS "content"
    )
    target_compile_definitions(CrowySampleStageBacklotTest
    PRIVATE
        CROWY_BACKLOT_DIR="${CROWY_BACKLOT_DIR}"
        CROWY_ENGINE_PROBE="${CMAKE_SOURCE_DIR}/Engine/Content/Test/Data/AxisProbe.fbx"
    )
    set_tests_properties(CrowySampleStageBacklotTest
    PROPERTIES
        SKIP_RETURN_CODE 77
    )
endif()
