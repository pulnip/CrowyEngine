# The editor's pieces, content-neutral: nothing here includes RenderApp or a
# content library, so the library can move to an editor executable of its own.
add_library(SampleEditor STATIC
    EditorCamera.cpp
    EditorPanels.cpp
    EditorPick.cpp
    EditorSession.cpp
)

target_include_directories(SampleEditor
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    # Camera.hpp, until a domain module owns the cameras
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(SampleEditor
PUBLIC
    Crowy::Core
    Crowy::Platform
    Crowy::Reflection
    Crowy::Render
    Crowy::Serialization
    imgui
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SampleEditor
        DIRECTORY Test
        DEPENDS SampleEditor
        LABELS "unit"
    )
endif()
