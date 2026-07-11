smol_declare_module(Platform)

target_link_libraries(CrowyPlatform
PUBLIC
    Crowy::Core
    Crowy::Serialization
    Crowy::RHI
PRIVATE
    SDL3::SDL3
    imgui
    Crowy::Log
)

smol_declare_private_interface(Platform)

if(SMOL_ENABLE_TEST)
    smol_declare_test(Platform
        DIRECTORY Test
        DEPENDS Crowy::Platform
        LABELS "unit"
    )
endif()
