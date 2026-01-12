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

if(CROWY_ENABLE_TEST)
    add_subdirectory(Sample)
endif()
