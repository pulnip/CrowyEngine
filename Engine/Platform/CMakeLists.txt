crowy_declare_module(Platform
PUBLIC
    Crowy::Core
    Crowy::Serialization
    Crowy::RHI
PRIVATE
    SDL3::SDL3
    imgui
    Crowy::Log
)

crowy_declare_private_interface(Platform)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Platform
        DIRECTORY Test
        DEPENDS Crowy::Platform
        LABELS "unit"
    )
endif()
