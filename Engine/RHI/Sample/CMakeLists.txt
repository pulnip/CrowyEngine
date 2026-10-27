add_subdirectory(Scene)
add_subdirectory(Terrain)
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
    Crowy::Content
)

crowy_declare_example(HelloCompute
    HEADLESS
SOURCES
    HelloCompute.cpp
)

crowy_declare_example(NormalMapping
SOURCES
    NormalMapping.cpp
DEPENDS
    Crowy::Content
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

crowy_declare_example(BlackholeSimulation
SOURCES
    BlackholeSimulation.cpp
)

crowy_declare_example(OrbitFrame
SOURCES
    OrbitFrame.cpp
DEPENDS
    Crowy::SampleOrbit
    Crowy::UI
)

crowy_declare_example(MarchingTerrain
SOURCES
    MarchingTerrain.cpp
DEPENDS
    Crowy::SampleTerrain
    Crowy::UI
)
