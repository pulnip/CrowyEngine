option(ImGui_DEMO
    "Include the ImGui demo window implementation in library"
    ON
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        "v1.92.5"
)
FetchContent_MakeAvailable(imgui)

add_library(ImGui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    $<$<BOOL:${ImGui_DEMO}>:${imgui_SOURCE_DIR}/imgui_demo.cpp>
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)

target_compile_definitions(ImGui
PUBLIC
    $<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>
)

target_include_directories(ImGui
PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(ImGui
PRIVATE
    SDL3::SDL3
)

if(ImGui_RENDER_BACKEND STREQUAL "D3D11")
    target_sources(ImGui PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
elseif(ImGui_RENDER_BACKEND STREQUAL "Metal")
    target_sources(ImGui PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
    )
    target_link_libraries(ImGui PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
    )
elseif(ImGui_RENDER_BACKEND STREQUAL "OpenGL3")
    target_sources(ImGui PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
else()
    message(FATAL_ERROR "ImGui_RENDER_BACKEND must be set.")
endif()

message(STATUS "ImGui configured with backend: ${ImGui_RENDER_BACKEND}")