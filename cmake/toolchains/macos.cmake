execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE MACOSX_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_OSX_SYSROOT "${MACOSX_SDK_PATH}" CACHE PATH "")

if(NOT DEFINED RENDER_BACKEND)
    set(RENDER_BACKEND "Metal")
endif()

if(RENDER_BACKEND STREQUAL "D3D12")
    message(FATAL_ERROR "D3D12 is not supported on macOS.")
elseif(RENDER_BACKEND STREQUAL "Metal")
    find_library(METAL_FRAMEWORK Metal)
elseif(RENDER_BACKEND STREQUAL "OpenGL")
    set(SHADER_COMPILER glslangValidator)
    find_package(OpenGL REQUIRED)
    find_package(glad REQUIRED)
else()
    message(FATAL_ERROR "Invalid RENDER_BACKEND ${RENDER_BACKEND}")
endif()
