add_library(CrowyECS STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyECS
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyECS
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyECS
PUBLIC
    Crowy::Core
    # NOTE: ARCHETYPES Definitions
    Crowy::EngineCore
    Crowy::Log
)

add_library(Crowy::ECS ALIAS CrowyECS)