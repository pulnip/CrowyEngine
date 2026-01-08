add_library(CrowyApp STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyApp
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyApp
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyApp
PUBLIC
    Crowy::Core
    Crowy::Platform
PRIVATE
    Crowy::ECS
    Crowy::Input
    Crowy::Render
    Crowy::Resource
    Crowy::Scene
    Crowy::Script
)

add_library(Crowy::App ALIAS CrowyApp)

add_subdirectory(Launcher)