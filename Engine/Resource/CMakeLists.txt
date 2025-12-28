add_library(CrowyResource STATIC)

target_sources(CrowyResource
PRIVATE
    Private/BinderRegistry.cpp
    Private/InternalComponentBinder.cpp
    Private/MaterialSetManager.cpp
    Private/MeshManager.cpp
    Private/ModelImporter.cpp
    Private/ModelSerializer.cpp
    Private/PathUtils.cpp
    Private/Resource.cpp
    Private/RenderParser.cpp
    Private/RenderPassBinder.cpp
    Private/TextureImporter.cpp
    Private/SceneParser.cpp
    Private/ShaderManager.cpp
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
    Crowy::EngineCore
    Crowy::RHI
PRIVATE
    Crowy::Log
    assimp::assimp
    tomlplusplus::tomlplusplus
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