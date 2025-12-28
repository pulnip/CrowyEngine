add_library(CrowyECS STATIC)

target_sources(CrowyECS
PRIVATE
    Private/ECSDefinitions.cpp
    Private/EntityRegistry.cpp
)

target_include_directories(CrowyECS
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyECS
PUBLIC
    Crowy::Core
    Crowy::Log
)

add_library(Crowy::ECS ALIAS CrowyECS)