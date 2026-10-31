# Backlot's film set as scene content; nothing else in Scene sees the Stage
# reader
crowy_declare_sample_library(BacklotImport
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    BacklotImport.cpp
PUBLIC
    Crowy::Core
    Crowy::SampleStage
    Crowy::Scene
    Crowy::WorldRender
TEST_DEPENDS
    Crowy::RenderTestSupport
    Crowy::Scene
    Crowy::WorldRender
)

# computes and writes files, so it opens no window
crowy_declare_example(ImportBacklot
    HEADLESS
SOURCES
    ImportBacklot.cpp
DEPENDS
    Crowy::SampleBacklotImport
    Crowy::Scene
    Crowy::WorldRender
)
set_source_files_properties(ImportBacklot.cpp
PROPERTIES
    COMPILE_DEFINITIONS "CROWY_BACKLOT_DIR=\"${CROWY_BACKLOT_DIR}\""
)

if(CROWY_ENABLE_TEST)
    # the imported set against the copy of the source it was made from, so it
    # needs no Backlot checkout
    crowy_declare_test(SampleBacklotContent
        DIRECTORY ContentTest
        DEPENDS
            Crowy::Core
            Crowy::Serialization
        LABELS "content"
    )
    target_compile_definitions(CrowySampleBacklotContentTest
    PRIVATE
        CROWY_CONTENT_ROOT="${CMAKE_SOURCE_DIR}/Content"
    )
endif()
