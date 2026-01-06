add_library(CrowyResource STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyResource
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyResource
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
PRIVATE
    "${stb_SOURCE_DIR}"
)

target_link_libraries(CrowyResource
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
    Crowy::EngineCore
    Crowy::Input
PRIVATE
    assimp::assimp
    tomlplusplus::tomlplusplus
    Crowy::Log
    Crowy::RHI
)

add_library(Crowy::Resource ALIAS CrowyResource)

add_library(CrowyResource_Private INTERFACE)

target_include_directories(CrowyResource_Private INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)

target_link_libraries(CrowyResource_Private INTERFACE Crowy::Resource)


if(CROWY_ENABLE_TEST)
    add_subdirectory(Test)
endif()