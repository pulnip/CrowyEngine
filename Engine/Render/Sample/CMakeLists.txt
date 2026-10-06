# the film set the stage samples and their tests read, a sibling checkout
# with Git LFS; tests skip when it is missing
cmake_path(ABSOLUTE_PATH CMAKE_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE CROWY_SOURCE_PARENT)
cmake_path(GET CROWY_SOURCE_PARENT PARENT_PATH CROWY_SOURCE_PARENT)
set(CROWY_BACKLOT_DIR "${CROWY_SOURCE_PARENT}/Backlot" CACHE PATH "Backlot's checkout")

add_subdirectory(Editor)
add_subdirectory(Paint)
if(CROWY_ENABLE_PHYSICS)
    add_subdirectory(PhysicsSync)
endif()
add_subdirectory(Stage)

# the render framework every windowed render sample, spike and the Island
# build on; App itself comes from each executable's FRAMEWORK
add_library(SampleRenderApp STATIC
    FlyCamera.cpp
    OrbitCamera.cpp
    RenderApp.cpp
)
target_include_directories(SampleRenderApp
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CROWY_APP_FRAMEWORK_DIR}"
)
target_link_libraries(SampleRenderApp
PUBLIC
    Crowy::Platform
    Crowy::Remote
    Crowy::Render
PRIVATE
    Crowy::Log
)

# the ImGui overlays, apart so samples without a UI skip ImGui
add_library(SampleRenderOverlay STATIC
    PortStatusChip.cpp
    StatsOverlay.cpp
)
target_link_libraries(SampleRenderOverlay
PUBLIC
    SampleRenderApp
    Crowy::UI
)

crowy_declare_example(TenThousandBlocks
SOURCES
    TenThousandBlocks.cpp
DEPENDS
    SampleRenderApp
)

crowy_declare_example(CornellRoom
SOURCES
    CornellRoom.cpp
DEPENDS
    Crowy::SampleScene
    SampleRenderApp
)

crowy_declare_example(Playground
SOURCES
    PlaygroundScene.cpp
    Playground.cpp
    RenderReflection.cpp
DEPENDS
    Crowy::Reflection
    SampleRenderOverlay
)

crowy_declare_example(ToonPlayground
SOURCES
    PlaygroundScene.cpp
    RenderReflection.cpp
    ToonPlayground.cpp
DEPENDS
    Crowy::Reflection
    SampleRenderOverlay
)

if(CROWY_ENABLE_PHYSICS)
    # the physics puzzles on a floor of their own, drawn one way through the
    # PhysicsSync glue
    crowy_declare_example(PhysicsPlayground
    SOURCES
        PhysicsPlayground.cpp
    DEPENDS
        Crowy::Reflection
        SamplePhysicsSync
        SamplePuzzle
        SampleRenderOverlay
    )
endif()

# MintChoco's scored, visible paint rebuilt one step at a time
crowy_declare_example(PaintLab
SOURCES
    PaintLab.cpp
    PaintLabControl.cpp
    PaintLabGpu.cpp
DEPENDS
    Crowy::Reflection
    SamplePaint
    SampleRenderOverlay
)

# Backlot's film set, read from the sibling checkout CROWY_BACKLOT_DIR names;
# exits 77 (skipped) without it
crowy_declare_example(StageEditor
SOURCES
    RenderReflection.cpp
    StageContent.cpp
    StageEditor.cpp
    StageInspect.cpp
DEPENDS
    Crowy::Reflection
    SampleEditor
    SampleRenderOverlay
    SampleStage
)
set_source_files_properties(StageEditor.cpp
PROPERTIES
    COMPILE_DEFINITIONS "CROWY_BACKLOT_DIR=\"${CROWY_BACKLOT_DIR}\""
)
if(TEST StageEditorSmoke)
    # the set loads for about 2.4 s in Debug before frame 1
    set_tests_properties(StageEditorSmoke
    PROPERTIES
        ENVIRONMENT "CROWY_SMOKE_DURATION=20"
    )
endif()

# every cut against its golden through the command port, which only
# Debug builds open; skipped when Backlot is not the stamped commit
set(STAGE_CUTS_OUT "${CMAKE_SOURCE_DIR}/captures/stage/ctest")
if(APPLE)
    set(STAGE_CUTS_COMMAND
        "${CMAKE_SOURCE_DIR}/Tools/stage_cuts.sh"
        --exe "$<TARGET_FILE:StageEditor>"
        --out "${STAGE_CUTS_OUT}"
    )
elseif(WIN32)
    set(STAGE_CUTS_COMMAND
        powershell -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_SOURCE_DIR}/Tools/stage_cuts.ps1"
        -Exe "$<TARGET_FILE:StageEditor>"
        -Out "${STAGE_CUTS_OUT}"
    )
endif()
if(DEFINED STAGE_CUTS_COMMAND)
    add_test(
        NAME StageEditorCuts
        CONFIGURATIONS Debug
        COMMAND ${STAGE_CUTS_COMMAND}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(StageEditorCuts
    PROPERTIES
        LABELS "smoke;stage"
        RUN_SERIAL TRUE
        TIMEOUT 300
        SKIP_RETURN_CODE 77
    )
endif()
