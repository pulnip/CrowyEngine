# fails the run if the CPU and GPU density fields drift apart
crowy_declare_check(TerrainDensityCheck
SOURCES
    TerrainDensityCheck.cpp
DEPENDS
    Crowy::SampleTerrain
)

# runs the marching passes once and checks the mesh landed on the isosurface
crowy_declare_check(TerrainMarchCheck
SOURCES
    TerrainMarchCheck.cpp
DEPENDS
    Crowy::SampleTerrain
)

# copies the trail ring back and holds it against the Kepler solver
crowy_declare_check(OrbitTrailCheck
SOURCES
    OrbitTrailCheck.cpp
DEPENDS
    SampleOrbit
)

# forces a creation-upload flush while a transient slice waits in an
# unsubmitted copy, and fails if the flush let the slice be overwritten
crowy_declare_check(UploadFlushCheck
SOURCES
    UploadFlushCheck.cpp
)
