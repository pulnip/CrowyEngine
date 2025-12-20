add_library(CrowyScene STATIC)

target_sources(CrowyScene
PRIVATE
    Private/SceneParser.cpp
)

target_include_directories(CrowyScene
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyScene
PUBLIC
    Crowy::Core
    Crowy::Log
PRIVATE
    tomlplusplus::tomlplusplus
)

add_library(Crowy::Scene ALIAS CrowyScene)

if(CROWY_ENABLE_TEST)
    add_subdirectory(Test)
endif()