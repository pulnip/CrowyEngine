crowy_declare_module(UI
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
    Crowy::Reflection
    imgui
PRIVATE
    ${RENDER_BACKENDS}
)

target_link_options(CrowyUI
PRIVATE
    ${RENDER_BACKEND_LINK_OPTIONS}
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(UI
        DIRECTORY Test
        DEPENDS Crowy::UI
        LABELS "unit"
    )

    add_subdirectory(Sample)
endif()
