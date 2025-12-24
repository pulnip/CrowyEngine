add_library(CrowyMetalRHI STATIC)

target_sources(CrowyMetalRHI
PRIVATE
    Private/MetalDevice.cpp
)

target_include_directories(CrowyMetalRHI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
PRIVATE
    "${metal-cpp_SOURCE_DIR}"
)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyMetalRHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyMetalRHI PUBLIC Crowy::RHI)
endif()

find_library(METAL_LIBRARY Metal REQUIRED)
find_library(METALKIT_LIBRARY MetalKit REQUIRED)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
find_library(FOUNDATION_LIBRARY Foundation REQUIRED)

target_link_libraries(CrowyMetalRHI
PUBLIC
    Crowy::CommonRHI
PRIVATE
    ${METAL_LIBRARY}
    ${METALKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${FOUNDATION_LIBRARY}
)

add_library(Crowy::MetalRHI ALIAS CrowyMetalRHI)