# No Crowy::World, deliberately: the host draws a RenderScene and knows no
# World. Executables get the D3D12 exports through crowy_declare_executable.
crowy_declare_module(App
PUBLIC
    Crowy::Asset
    Crowy::Core
    Crowy::Platform
    Crowy::Reflection
    Crowy::Remote
    Crowy::Render
    Crowy::RHI
    Crowy::Serialization
    Crowy::UI
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(App
        DIRECTORY Test
        DEPENDS
            Crowy::App
            Crowy::WorldRender
        LABELS "unit"
    )
endif()
