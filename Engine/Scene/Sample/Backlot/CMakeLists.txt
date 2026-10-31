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
