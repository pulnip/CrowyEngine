# guards the shared-pool vertex pulling contract: a mesh's pool offset travels
# in DrawData::vbIndex, never in the args' baseVertex
crowy_declare_spike(VertexPullSpike
SOURCES
    # the render framework and its camera still live with the samples;
    # they move the day a domain module owns them
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample/FlyCamera.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample/RenderApp.cpp"
    VertexPullSpike.cpp
DEPENDS
    Crowy::Render
)

target_include_directories(VertexPullSpike
PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample"
)
