include(FetchContent)

find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "release-3.2.30"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    # Library Type
    set(SDL_SHARED ON  CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    # Feature
    set(SDL_AUDIO    OFF CACHE BOOL "" FORCE)
    set(SDL_VIDEO    ON  CACHE BOOL "" FORCE)
    set(SDL_GPU      OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGL   OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN   OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER   OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA   OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC   OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI   OFF CACHE BOOL "" FORCE)
    set(SDL_POWER    OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR   OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG   OFF CACHE BOOL "" FORCE)
    # Test Build
    set(SDL_TESTS         OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY  OFF CACHE BOOL "" FORCE)
    # Install
    set(SDL_INSTALL       OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
    if(TARGET SDL3-shared)
        target_compile_options(SDL3-shared PRIVATE
            $<$<C_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        )
    elseif(TARGET SDL3-static)
        target_compile_options(SDL3-static PRIVATE
            $<$<C_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        )
    endif()
endif()

# stb - header-only image loading library
FetchContent_Declare(
    stb
    GIT_REPOSITORY "https://github.com/nothings/stb.git"
    GIT_TAG "31c1ad37456438565541f4919958214b6e762fb4"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

add_library(stb INTERFACE)

target_include_directories(stb
SYSTEM INTERFACE
    "${stb_SOURCE_DIR}"
)

# tomlplusplus - TOML parser library
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY "https://github.com/marzer/tomlplusplus.git"
    GIT_TAG "v3.4.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

if(WIN32)
    set(AGILITY_SDK_VERSION "1.619.4")

    # DirectX Agility SDK
    FetchContent_Declare(
        AgilitySDK
        URL      "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${AGILITY_SDK_VERSION}"
        URL_HASH SHA256=d30f756ce05bb4b7705fc1b04a5ded32ed62f2c2a2b392ae8d3318181395c8bc
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(AgilitySDK)

    set(AGILITY_SDK_BIN_DIR "${agilitysdk_SOURCE_DIR}/build/native/bin/x64")
elseif(APPLE)
    # Metal-cpp
    FetchContent_Declare(
        metal-cpp
        URL https://developer.apple.com/metal/cpp/files/metal-cpp_macOS15_iOS18.zip
    )
    FetchContent_MakeAvailable(metal-cpp)
endif()

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        "v1.92.5"
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC)

target_sources(imgui
PUBLIC
    ${imgui_SOURCE_DIR}/imgui.h
PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    $<$<BOOL:${SMOL_ENABLE_TEST}>:${imgui_SOURCE_DIR}/imgui_demo.cpp>
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)

target_include_directories(imgui
PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui
PRIVATE
    SDL3::SDL3
)

if(WIN32)
    target_sources(imgui
    PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    )
elseif(APPLE)
    target_compile_options(imgui
    PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-arc-bridge-casts-disallowed-in-nonarc>
    )

    target_compile_definitions(imgui
    PUBLIC
        IMGUI_IMPL_METAL_CPP
    )

    target_sources(imgui
    PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
    )
    target_include_directories(imgui
    SYSTEM PUBLIC
        "${metal-cpp_SOURCE_DIR}"
    )
    target_link_libraries(imgui
    PRIVATE
        ${METAL_LIBRARY}
    )
endif()

# Slang
set(SLANG_VERSION "2026.13")
if(WIN32)
    set(SLANG_TARGET "windows-x86_64")
elseif(APPLE)
    set(SLANG_TARGET "macos-aarch64")
endif()
FetchContent_Declare(
    slang_bin
    URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_TARGET}.zip"
)
FetchContent_MakeAvailable(slang_bin)

if(SMOL_ENABLE_TEST)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        FetchContent_Declare(
            GTest
            GIT_REPOSITORY "https://github.com/google/googletest.git"
            GIT_TAG "v1.17.0"
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(GTEST_HAS_ABSL OFF CACHE BOOL "" FORCE)
        add_compile_options(
            $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-character-conversion>
        )
        FetchContent_MakeAvailable(GTest)
    endif()
endif()
