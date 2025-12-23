add_library(CrowyCommonRHI INTERFACE)

target_include_directories(CrowyCommonRHI
INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Common"
)

add_library(Crowy::CommonRHI ALIAS CrowyCommonRHI)

if(CROWY_USE_STATIC_RHI)
    add_library(CrowyRHI INTERFACE)

    target_include_directories(CrowyRHI
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
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
        message(FATAL_ERROR "No RHI Implementation for ${RENDER_BACKEND}")
    endif()
else()
    add_library(CrowyRHI STATIC)

    target_sources(CrowyRHI
    PRIVATE
        Private/RHIDevice.cpp
    )

    target_include_directories(CrowyRHI
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
    )

    target_link_libraries(CrowyRHI
    PUBLIC
        Crowy::CommonRHI
        Crowy::Core
    )
endif()

add_library(Crowy::RHI ALIAS CrowyRHI)