add_subdirectory(Editor)
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
    ToonPlayground.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
    Crowy::Reflection
    Crowy::UI
)

# Backlot's film set, read from the sibling checkout CROWY_BACKLOT_DIR names;
# exits 77 (skipped) without it
crowy_declare_example(StageEditor
SOURCES
    RenderApp.cpp
    StageContent.cpp
    StageEditor.cpp
DEPENDS
    Crowy::Reflection
    Crowy::Remote
    Crowy::Render
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
endif()
