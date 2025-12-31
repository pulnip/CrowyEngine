add_library(CrowyNullRHI STATIC)

target_sources(CrowyNullRHI
PUBLIC
    Public/NullDevice.hpp
PRIVATE
    Private/NullDevice.cpp
    Private/NullBuffer.hpp
    Private/NullFence.hpp
    Private/NullPipelineState.hpp
    Private/NullShader.hpp
    Private/NullSwapchain.hpp
    Private/NullTexture.hpp
)

target_include_directories(CrowyNullRHI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyNullRHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyNullRHI PUBLIC Crowy::RHI)
endif()

target_link_libraries(CrowyNullRHI
PUBLIC
    Crowy::CommonRHI
)

add_library(Crowy::NullRHI ALIAS CrowyNullRHI)