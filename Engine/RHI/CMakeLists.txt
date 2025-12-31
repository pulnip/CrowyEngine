add_library(CrowyCommonRHI INTERFACE)

file(GLOB COMMON_SOURCES "Common/*.h")

target_sources(CrowyCommonRHI
INTERFACE
    ${COMMON_SOURCES}
)

target_include_directories(CrowyCommonRHI
INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Common"
)

add_library(Crowy::CommonRHI ALIAS CrowyCommonRHI)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

if(CROWY_USE_STATIC_RHI)
    add_library(CrowyRHI INTERFACE)

    target_sources(CrowyRHI
    INTERFACE
        ${PUBLIC_SOURCES}
    )

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
        target_link_libraries(CrowyRHI INTERFACE Crowy::NullRHI)
    endif()
else()
    add_library(CrowyRHI STATIC)

    target_sources(CrowyRHI
    PUBLIC
        ${PUBLIC_SOURCES}
    PRIVATE
        ${PRIVATE_SOURCES}
    )

    target_include_directories(CrowyRHI
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
    )

    target_link_libraries(CrowyRHI
    PUBLIC
        Crowy::CommonRHI
        Crowy::Core
    PRIVATE
        Crowy::Log
    )
endif()

add_library(Crowy::RHI ALIAS CrowyRHI)