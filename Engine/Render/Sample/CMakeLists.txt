add_subdirectory(Editor)
add_subdirectory(PhysicsSync)
add_subdirectory(Stage)

crowy_declare_example(TenThousandBlocks
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    TenThousandBlocks.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
)

crowy_declare_example(CornellRoom
SOURCES
    OrbitCamera.cpp
    RenderApp.cpp
    CornellRoom.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
    Crowy::SampleScene
)

crowy_declare_example(Playground
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    PortStatusChip.cpp
    StatsOverlay.cpp
    PlaygroundScene.cpp
    Playground.cpp
    RenderReflection.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
    Crowy::Reflection
    Crowy::UI
)

crowy_declare_example(ToonPlayground
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    PortStatusChip.cpp
    StatsOverlay.cpp
    PlaygroundScene.cpp
    RenderReflection.cpp
    ToonPlayground.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
    Crowy::Reflection
    Crowy::UI
)

# the physics puzzles on a floor of their own, drawn one way through the
# PhysicsSync glue
crowy_declare_example(PhysicsPlayground
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    PortStatusChip.cpp
    StatsOverlay.cpp
    PhysicsPlayground.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
    Crowy::Reflection
    Crowy::UI
    SamplePhysicsSync
    SamplePuzzle
)

# Backlot's film set, read from the sibling checkout CROWY_BACKLOT_DIR names;
# exits 77 (skipped) without it
crowy_declare_example(StageEditor
SOURCES
    PortStatusChip.cpp
    RenderApp.cpp
    RenderReflection.cpp
    StageContent.cpp
    StageEditor.cpp
    StageInspect.cpp
DEPENDS
    Crowy::Reflection
    Crowy::Remote
    Crowy::Render
    Crowy::UI
    SampleEditor
    SampleStage
)
target_compile_definitions(StageEditor
PRIVATE
    CROWY_BACKLOT_DIR="${CROWY_BACKLOT_DIR}"
)
if(TARGET StageEditorSmoke OR CROWY_ENABLE_TEST)
    # the set loads for about 2.4 s in Debug before frame 1
    set_tests_properties(StageEditorSmoke
    PROPERTIES
        ENVIRONMENT "CROWY_SMOKE_DURATION=20"
    )

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
endif()
