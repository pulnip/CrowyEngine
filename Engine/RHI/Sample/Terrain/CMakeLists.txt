add_library(CrowySampleTerrain STATIC
    MarchingCubes.cpp
    TerrainCamera.cpp
    TerrainMarch.cpp
    TerrainSpan.cpp
    TerrainSurface.cpp
    TerrainUI.cpp
)

target_include_directories(CrowySampleTerrain
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(CrowySampleTerrain
PUBLIC
    # TerrainMarch.hpp hands out command list and resource types
    Crowy::RHI
    # TerrainCamera reads the input provider
    Crowy::Platform
    Crowy::UI
)

add_library(Crowy::SampleTerrain ALIAS CrowySampleTerrain)
