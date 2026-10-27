crowy_declare_check(TerrainDensityCheck
    TEST gpu
SOURCES
    TerrainDensityCheck.cpp
DEPENDS
    Crowy::SampleTerrain
)

crowy_declare_check(TerrainMarchCheck
    TEST gpu
SOURCES
    TerrainMarchCheck.cpp
DEPENDS
    Crowy::SampleTerrain
)

crowy_declare_check(OrbitTrailCheck
    TEST gpu
SOURCES
    OrbitTrailCheck.cpp
DEPENDS
    Crowy::SampleOrbit
)

crowy_declare_check(UploadFlushCheck
    TEST gpu
SOURCES
    UploadFlushCheck.cpp
)
