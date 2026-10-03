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
    Crowy::Remote
    Crowy::Render
)

target_include_directories(VertexPullSpike
PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample"
)

# guards position invariance between the depth prepass and the opaque pass
# that tests Equal against it
crowy_declare_spike(PrepassEqualSpike
SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample/FlyCamera.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample/RenderApp.cpp"
    PrepassEqualSpike.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
)

target_include_directories(PrepassEqualSpike
PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample"
)

# guards the standard pipeline's textures: a point-sampled palette, an atlas
# rect through the material's UV transform, an emissive map, and a surface
# that refuses the shadow it stands in
crowy_declare_spike(TexturedSpike
SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample/FlyCamera.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample/RenderApp.cpp"
    TexturedSpike.cpp
DEPENDS
    Crowy::Remote
    Crowy::Render
)

target_include_directories(TexturedSpike
PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sample"
)
