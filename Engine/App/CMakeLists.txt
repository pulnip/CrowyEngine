crowy_declare_module(App)

target_link_libraries(CrowyApp
PUBLIC
    Crowy::Core
    Crowy::Platform
PRIVATE
    Crowy::ECS
    Crowy::Input
    Crowy::Log
    Crowy::Render
    Crowy::Resource
    Crowy::Scene
    Crowy::Script
    Crowy::UI
)

add_subdirectory(Launcher)