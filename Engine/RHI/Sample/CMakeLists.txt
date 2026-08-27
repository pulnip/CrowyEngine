# ready-made scenes the samples can render
add_subdirectory(Scene)
# density field, noise and UI panel the two terrain samples share
add_subdirectory(Terrain)
# Keplerian orbits, trail ring buffer and panel for OrbitFrame
add_subdirectory(Orbit)

crowy_declare_example(HelloWorld
SOURCES
    HelloWorld.cpp
)

crowy_declare_example(HelloConstantBuffer
SOURCES
    HelloConstantBuffer.cpp
)

crowy_declare_example(HelloTexture
SOURCES
    HelloTexture.cpp
DEPENDS
    Crowy::Serialization
)

# computes and exits, so it opens no window
crowy_declare_example(HelloCompute
    HEADLESS
SOURCES
    HelloCompute.cpp
)

crowy_declare_example(HelloIndirectDraw
SOURCES
    HelloIndirectDraw.cpp
)

crowy_declare_example(CubeMapping
SOURCES
    CubeMapping.cpp
DEPENDS
    Crowy::Serialization
)

crowy_declare_example(NormalMapping
SOURCES
    NormalMapping.cpp
DEPENDS
    Crowy::Serialization
)

crowy_declare_example(CornellBox
SOURCES
    CornellBox.cpp
DEPENDS
    Crowy::SampleScene
)

crowy_declare_example(SkyboxFloor
SOURCES
    SkyboxFloor.cpp
DEPENDS
    Crowy::SampleScene
)

crowy_declare_example(Checkerboard
SOURCES
    Checkerboard.cpp
)

crowy_declare_example(TenThousandCubes
SOURCES
    TenThousandCubes.cpp
DEPENDS
    Crowy::Serialization
)

crowy_declare_example(BlackholeSimulation
SOURCES
    BlackholeSimulation.cpp
)

crowy_declare_example(OrbitFrame
SOURCES
    OrbitFrame.cpp
DEPENDS
    SampleOrbit
    Crowy::UI
)

crowy_declare_example(SpanTerrain
SOURCES
    SpanTerrain.cpp
DEPENDS
    Crowy::SampleTerrain
    Crowy::UI
)

crowy_declare_example(MarchingTerrain
SOURCES
    MarchingTerrain.cpp
DEPENDS
    Crowy::SampleTerrain
    Crowy::UI
)
