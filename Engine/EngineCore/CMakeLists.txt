add_library(CrowyEngineCore STATIC)

target_sources(CrowyEngineCore
PUBLIC
    Public/ComponentDefinitions.hpp
    Public/ECSDefinitions.hpp
    Public/RenderDefinitions.hpp
    Public/ResourceHandle.hpp
PRIVATE
    Private/ECSDefinitions.cpp
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