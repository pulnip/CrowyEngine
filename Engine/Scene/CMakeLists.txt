add_library(CrowyScene STATIC)

target_sources(CrowyScene
PRIVATE
    Private/SceneLoader.cpp
)

target_include_directories(CrowyScene
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyScene
PUBLIC
    Crowy::Core
    Crowy::EngineCore
    Crowy::ECS
    Crowy::Resource
PRIVATE
    Crowy::Log
)

add_library(Crowy::Scene ALIAS CrowyScene)

add_library(CrowyScene_Private INTERFACE)
target_include_directories(CrowyScene_Private
INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)
target_link_libraries(CrowyScene_Private
INTERFACE
    Crowy::Scene
)

if(CROWY_ENABLE_TEST)
    add_subdirectory(Test)
endif()