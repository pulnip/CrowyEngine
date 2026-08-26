crowy_graphics_sample(TenThousandBlocks
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    TenThousandBlocks.cpp
DEPENDS
    Crowy::Render
)

crowy_graphics_sample(CornellRoom
SOURCES
    OrbitCamera.cpp
    RenderApp.cpp
    CornellRoom.cpp
DEPENDS
    Crowy::Render
    Crowy::SampleScene
)

crowy_graphics_sample(MaterialBuckets
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    MaterialBuckets.cpp
DEPENDS
    Crowy::Render
    Crowy::Reflection
    Crowy::UI
)

# guards the shared-pool vertex pulling contract: a mesh's pool offset travels
# in DrawData::vbIndex, never in the args' baseVertex
crowy_graphics_sample(VertexPullSpike
SOURCES
    FlyCamera.cpp
    RenderApp.cpp
    VertexPullSpike.cpp
DEPENDS
    Crowy::Render
)
