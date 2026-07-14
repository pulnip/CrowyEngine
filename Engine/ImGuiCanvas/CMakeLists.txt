crowy_declare_module(ImGuiCanvas)

target_link_libraries(CrowyImGuiCanvas
PUBLIC
    Crowy::Canvas2D
PRIVATE
    imgui
    Crowy::Log
)
