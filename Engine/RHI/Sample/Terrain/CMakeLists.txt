add_library(CrowySampleTerrain STATIC
    TerrainUI.cpp
)

target_include_directories(CrowySampleTerrain
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(CrowySampleTerrain
PUBLIC
    Crowy::UI
)

add_library(Crowy::SampleTerrain ALIAS CrowySampleTerrain)
