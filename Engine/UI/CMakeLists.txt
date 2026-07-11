smol_declare_module(UI)

target_link_options(CrowyUI
PRIVATE
    ${RENDER_BACKEND_LINK_OPTIONS}
)
target_link_libraries(CrowyUI
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
    imgui
PRIVATE
    ${RENDER_BACKENDS}
)
