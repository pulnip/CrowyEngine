add_library(CrowyResource STATIC)

target_sources(CrowyResource
PRIVATE
    Private/MaterialSetManager.cpp
    Private/MeshImporter.cpp
    Private/MeshManager.cpp
    Private/MeshSerializer.cpp
    Private/Resource.cpp
    Private/ShaderManager.cpp
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