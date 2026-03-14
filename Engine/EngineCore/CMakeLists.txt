crowy_declare_module(EngineCore)

target_link_libraries(CrowyEngineCore
PUBLIC
    Crowy::Core
    Crowy::CommonRHI
)

crowy_rhi_macro(EngineCore)