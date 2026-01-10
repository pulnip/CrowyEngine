crowy_declare_module(App)

target_link_libraries(CrowyApp
PUBLIC
    Crowy::Core
    Crowy::Platform
PRIVATE
    Crowy::ECS
    Crowy::Input
    Crowy::Render
    Crowy::Resource
    Crowy::Scene
    Crowy::Script
)

add_subdirectory(Launcher)