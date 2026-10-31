# StageEditor's session and panels, frozen until the real editor opens Backlot
# and they go with it.
crowy_declare_sample_library(Editor
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    EditorPanels.cpp
    EditorSession.cpp
PUBLIC
    Crowy::Content
    Crowy::Core
    Crowy::Editor
    Crowy::Platform
    Crowy::Reflection
    Crowy::Render
    Crowy::UI
    imgui
TEST_DEPENDS
    Crowy::RenderTestSupport
)
