crowy_declare_module(UI
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
    imgui
PRIVATE
    ${RENDER_BACKENDS}
)

target_link_options(CrowyUI
PRIVATE
    ${RENDER_BACKEND_LINK_OPTIONS}
)
