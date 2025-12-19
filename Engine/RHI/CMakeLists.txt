add_library(CrowyRHI STATIC)

target_sources(CrowyRHI
PRIVATE
    Private/RHIDevice.cpp
)

target_include_directories(CrowyRHI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

add_library(Crowy::RHI ALIAS CrowyRHI)