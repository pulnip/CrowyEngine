set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Compiler Config
add_library(CrowyProjectInterface INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        # clang-cl config
        # use Exception Handling for stack unwinding
        add_compile_options(
            /EHsc
        )
        add_compile_definitions(
            NOMINMAX
            VC_EXTRALEAN
            WIN32_LEAN_AND_MEAN
        )
        target_compile_options(CrowyProjectInterface
        INTERFACE
            /utf-8
        )
    else()
        # pure Clang or AppleClang config here
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
        /EHsc
        /MP
        /Zc:preprocessor
    )
    add_compile_definitions(
        NOMINMAX
        VC_EXTRALEAN
        WIN32_LEAN_AND_MEAN
    )
    target_compile_options(CrowyProjectInterface
    INTERFACE
        /utf-8
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(CrowyProjectInterface
    INTERFACE
        -finput-charset=UTF-8
    )
endif()

set(RENDER_BACKEND_LINK_OPTIONS "")
set(RENDER_BACKENDS "")
set(CANVAS_BACKENDS "")

# OS config
if(WIN32)
    # find dxc compiler
    file(GLOB WINDOWS_SDK_REDIST_D3D_PATHS
        "$ENV{WindowsSdkDir}/Redist/D3D/x64"
        "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64"
    )
    find_file(DXCOMPILER_DLL
        NAMES dxcompiler.dll
        HINTS ${WINDOWS_SDK_REDIST_D3D_PATHS}
    )
    find_file(DXIL_DLL
        NAMES dxil.dll
        HINTS ${WINDOWS_SDK_REDIST_D3D_PATHS}
    )
    if(NOT DXCOMPILER_DLL OR NOT DXIL_DLL)
        message(FATAL_ERROR
            "dxcompiler.dll / dxil.dll not found under Windows SDK Redist/D3D. "
            "Please check your Windows SDK installation."
        )
    endif()

    list(APPEND RENDER_BACKEND_LINK_OPTIONS /DELAYLOAD:d3d12.dll)
    list(APPEND RENDER_BACKENDS Crowy::DX12RHI delayimp.lib)
    list(APPEND CANVAS_BACKENDS Crowy::ImGuiCanvas)
elseif(APPLE)
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE MACOSX_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CMAKE_OSX_SYSROOT "${MACOSX_SDK_PATH}")

    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(METALKIT_LIBRARY MetalKit REQUIRED)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)

    list(APPEND RENDER_BACKENDS Crowy::MetalRHI)
    list(APPEND CANVAS_BACKENDS Crowy::ImGuiCanvas)
endif()

include(deps)
include(util)

if(CROWY_ENABLE_TEST)
    enable_testing()
    include(CTest)
    include(GoogleTest)
endif()