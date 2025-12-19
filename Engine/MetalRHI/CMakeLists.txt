set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
enable_language(Swift)

# Swift implementation
add_library(CrowyMetalSwiftRHI STATIC)

set_target_properties(CrowyMetalSwiftRHI PROPERTIES
    Swift_MODULE_NAME "CrowyMetalRHI"
    Swift_MODULE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/swiftmodules"
)

target_compile_options(CrowyMetalSwiftRHI
PRIVATE
    "-import-objc-header" "${CMAKE_CURRENT_SOURCE_DIR}/Private/CrowyMetalSwiftRHI-Bridging-Header.h"
)

target_sources(CrowyMetalSwiftRHI
PRIVATE
    Private/MetalDevice.swift
)

find_library(METAL_LIBRARY Metal REQUIRED)
find_library(METALKIT_LIBRARY MetalKit REQUIRED)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
find_library(FOUNDATION_LIBRARY Foundation REQUIRED)

target_link_libraries(CrowyMetalSwiftRHI
PUBLIC
    Crowy::RHI
PRIVATE
    ${METAL_LIBRARY}
    ${METALKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${FOUNDATION_LIBRARY}
)

add_library(Crowy::MetalSwiftRHI ALIAS CrowyMetalSwiftRHI)

# C++ Wrapper
add_library(CrowyMetalRHI STATIC)

target_sources(CrowyMetalRHI
PRIVATE
    Private/MetalDevice.cpp
)

target_include_directories(CrowyMetalRHI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyMetalRHI
PUBLIC
    Crowy::RHI
PRIVATE
    Crowy::MetalSwiftRHI
)

add_library(Crowy::MetalRHI ALIAS CrowyMetalRHI)