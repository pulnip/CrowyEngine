include(FetchContent)

find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "release-3.2.26"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
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
find_package(stb QUIET)
if(NOT stb_FOUND)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY "https://github.com/nothings/stb.git"
        GIT_TAG "31c1ad37456438565541f4919958214b6e762fb4"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stb)
endif()

# tomlplusplus - TOML parser library
find_package(tomlplusplus QUIET)
if(NOT tomlplusplus_FOUND)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY "https://github.com/marzer/tomlplusplus.git"
        GIT_TAG "v3.4.0"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)
endif()

# assimp - 3D model loading library
find_package(assimp QUIET)
if(NOT assimp_FOUND)
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY "https://github.com/assimp/assimp.git"
        GIT_TAG "v6.0.2"
        GIT_SHALLOW TRUE
    )
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_MMD_IMPORTER ON CACHE BOOL "" FORCE)  # PMX/PMD support
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(assimp)
    if(APPLE AND TARGET assimp)
        get_target_property(_incs assimp INCLUDE_DIRECTORIES)
        if(_incs)
            # for Homebrew LLVM
            list(FILTER _incs EXCLUDE REGEX "MacOSX\\.sdk")
            set_target_properties(assimp PROPERTIES INCLUDE_DIRECTORIES "${_incs}")
        endif()
    endif()

    if(TARGET zlibstatic)
        target_compile_options(zlibstatic PRIVATE
            $<$<C_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-non-prototype>
        )
    endif()
endif()

# angelscript - scripting library
find_package(angelscript QUIET)
if(NOT angelscript_FOUND)
    FetchContent_Declare(
        angelscript
        URL https://www.angelcode.com/angelscript/sdk/files/angelscript_2.38.0.zip
    )
    FetchContent_MakeAvailable(angelscript)
    set(AS_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory(
        ${angelscript_SOURCE_DIR}/angelscript/projects/cmake
        ${angelscript_BINARY_DIR}
    )
    target_compile_options(angelscript
    PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-fno-strict-aliasing>
    )
    if(APPLE)
        target_compile_options(angelscript
        PRIVATE
            $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        )
    endif()
endif()

# Metal-cpp
if(RENDER_BACKEND STREQUAL "Metal")
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
    $<$<BOOL:${CROWY_ENABLE_TEST}>:${imgui_SOURCE_DIR}/imgui_demo.cpp>
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

if(RENDER_BACKEND STREQUAL "Metal")
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
elseif(RENDER_BACKEND STREQUAL "D3D11")
    target_sources(imgui
    PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
elseif(RENDER_BACKEND STREQUAL "D3D12")
    target_sources(imgui
    PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    )
endif()

if(CROWY_ENABLE_TEST)
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
