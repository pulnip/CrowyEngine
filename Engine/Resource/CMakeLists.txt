add_library(CrowyResource STATIC)

target_sources(CrowyResource
PRIVATE
    Private/Material.cpp
    Private/MaterialSet.cpp
    Private/Mesh.cpp
    Private/MeshImporter.cpp
    Private/MeshSerializer.cpp
    Private/Shader.cpp
    Private/Submesh.cpp
    Private/TextureImporter.cpp
)

target_include_directories(CrowyResource
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
PRIVATE
    "${stb_SOURCE_DIR}"
)

target_link_libraries(CrowyResource
PUBLIC
    Crowy::Core
    Crowy::RHI
PRIVATE
    Crowy::Log
    assimp::assimp
)

add_library(Crowy::Resource ALIAS CrowyResource)