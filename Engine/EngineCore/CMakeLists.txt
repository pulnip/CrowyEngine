add_library(CrowyEngineCore STATIC)

target_sources(CrowyEngineCore
PRIVATE
    Private/ECSDefinitions.cpp
    Private/RenderDefinitions.cpp
)

target_include_directories(CrowyEngineCore
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyEngineCore
PUBLIC
    Crowy::Core
)

add_library(Crowy::EngineCore ALIAS CrowyEngineCore)