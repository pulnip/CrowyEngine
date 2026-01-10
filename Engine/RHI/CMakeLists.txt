crowy_declare_interface(CommonRHI
    DIRECTORY Common
)

if(CROWY_USE_STATIC_RHI)
    crowy_declare_interface(RHI
        DIRECTORY Public
    )

    target_link_libraries(CrowyRHI
    INTERFACE
        Crowy::CommonRHI
        Crowy::Core
    )

    if(RENDER_BACKEND STREQUAL "Metal")
        target_compile_definitions(CrowyRHI INTERFACE USE_METAL_BACKEND)
        target_link_libraries(CrowyRHI INTERFACE Crowy::MetalRHI)
    else()
        target_link_libraries(CrowyRHI INTERFACE Crowy::NullRHI)
    endif()
else()
    crowy_declare_module(RHI)

    target_link_libraries(CrowyRHI
    PUBLIC
        Crowy::CommonRHI
        Crowy::Core
    PRIVATE
        Crowy::Log
    )
endif()

if(CROWY_ENABLE_TEST)
    add_subdirectory(Sample)

    crowy_declare_test(RHI
        DIRECTORY Test
        DEPENDS $<IF:$<PLATFORM_ID:Darwin>,Crowy::MetalRHI,Crowy::NullRHI>
        LABELS "unit"
    )
endif()