crowy_declare_module(Editor
PUBLIC
    Crowy::Content
    Crowy::Core
    Crowy::Platform
    Crowy::Reflection
    Crowy::Remote
    Crowy::Render
    Crowy::UI
    Crowy::World
    Crowy::WorldRender
)

if(CROWY_ENABLE_TEST)
    # the gizmo and picking tests, moved here with what they cover
    crowy_declare_test(Editor
        DIRECTORY Test
        DEPENDS
            Crowy::Editor
            Crowy::RenderTestSupport
        LABELS "unit"
    )
endif()
