# the film set the stage samples and their tests read, a sibling checkout
# with Git LFS; tests skip when it is missing
cmake_path(ABSOLUTE_PATH CMAKE_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE CROWY_SOURCE_PARENT)
cmake_path(GET CROWY_SOURCE_PARENT PARENT_PATH CROWY_SOURCE_PARENT)
set(CROWY_BACKLOT_DIR "${CROWY_SOURCE_PARENT}/Backlot" CACHE PATH "Backlot's checkout")

add_subdirectory(Editor)
if(CROWY_ENABLE_PHYSICS)
    add_subdirectory(PhysicsSync)
endif()
add_subdirectory(Stage)

# the render framework every windowed render sample, spike and the Island
# build on; App itself comes from each executable's FRAMEWORK
crowy_declare_sample_library(RenderApp
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    FlyCamera.cpp
    FlyControl.cpp
    FrameCaptures.cpp
    FrameHost.cpp
    FramePort.cpp
    FrameStatsLog.cpp
    OrbitCamera.cpp
    RenderApp.cpp
INCLUDE
    "${CROWY_APP_FRAMEWORK_DIR}"
PUBLIC
    Crowy::Platform
    Crowy::Remote
    Crowy::Render
PRIVATE
    Crowy::Log
TEST_DEPENDS
    Crowy::WorldRender
)

# the ImGui overlays, apart so samples without a UI skip ImGui
crowy_declare_sample_library(RenderOverlay
SOURCES
    PortStatusChip.cpp
    StatsOverlay.cpp
PUBLIC
    Crowy::SampleRenderApp
    Crowy::UI
)

crowy_declare_example(TenThousandBlocks
SOURCES
    TenThousandBlocks.cpp
DEPENDS
    Crowy::WorldRender
    Crowy::SampleRenderApp
)

crowy_declare_example(CornellRoom
SOURCES
    CornellRoom.cpp
DEPENDS
    Crowy::SampleScene
    Crowy::SampleRenderApp
)

crowy_declare_example(Playground
    TEST app
SOURCES
    PlaygroundScene.cpp
    Playground.cpp
DEPENDS
    Crowy::Reflection
    Crowy::WorldRender
    Crowy::SampleRenderOverlay
)

crowy_declare_example(ToonPlayground
SOURCES
    PlaygroundScene.cpp
    ToonPlayground.cpp
DEPENDS
    Crowy::Reflection
    Crowy::WorldRender
    Crowy::SampleRenderOverlay
)

if(CROWY_ENABLE_PHYSICS)
    # the physics puzzles on a floor of their own, drawn one way through the
    # PhysicsSync glue
    crowy_declare_example(PhysicsPlayground
        TEST app
        # past the 90-frame lead-in, so the run steps the puzzles too
        FRAMES 120
    SOURCES
        PhysicsPlayground.cpp
    DEPENDS
        Crowy::Reflection
        Crowy::SamplePhysicsSync
        Crowy::SamplePuzzle
        Crowy::SampleRenderOverlay
    )
endif()

# Backlot's film set, read from the sibling checkout CROWY_BACKLOT_DIR names;
# exits 77 (skipped) without it
crowy_declare_example(StageEditor
    TEST app
SOURCES
    StageContent.cpp
    StageEditor.cpp
DEPENDS
    Crowy::Reflection
    Crowy::SampleEditor
    Crowy::SampleRenderOverlay
    Crowy::SampleStage
)
set_source_files_properties(StageEditor.cpp
PROPERTIES
    COMPILE_DEFINITIONS "CROWY_BACKLOT_DIR=\"${CROWY_BACKLOT_DIR}\""
)
