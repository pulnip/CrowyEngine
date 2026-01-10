crowy_declare_module(Render)

target_link_libraries(CrowyRender
PUBLIC
    Crowy::CommonRHI
    Crowy::EngineCore
PRIVATE
    Crowy::Log
    Crowy::Resource
    Crowy::RHI
)
