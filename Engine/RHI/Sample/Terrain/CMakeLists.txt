crowy_declare_sample_library(Terrain
SOURCES
    MarchingCubes.cpp
    TerrainCamera.cpp
    TerrainMarch.cpp
    TerrainSpan.cpp
    TerrainSurface.cpp
    TerrainUI.cpp
PUBLIC
    # TerrainMarch.hpp hands out command list and resource types
    Crowy::RHI
    # TerrainCamera reads the input provider
    Crowy::Platform
    Crowy::UI
)
