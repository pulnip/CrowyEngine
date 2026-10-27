# The editor's pieces, content-neutral: nothing here includes RenderApp or a
# content library, so the library can move to an editor executable of its own.
crowy_declare_sample_library(Editor
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    EditorCamera.cpp
    EditorGizmo.cpp
    EditorPanels.cpp
    EditorPick.cpp
    EditorSession.cpp
INCLUDE
    # Camera.hpp lives in the sample framework
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
PUBLIC
    Crowy::Content
    Crowy::Core
    Crowy::Platform
    Crowy::Reflection
    Crowy::Render
    Crowy::UI
    imgui
)
