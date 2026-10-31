# guards the shared-pool vertex pulling contract: a mesh's pool offset travels
# in DrawData::vbIndex, never in the args' baseVertex
crowy_declare_spike(VertexPullSpike
    TEST render
SOURCES
    VertexPullSpike.cpp
)

# guards position invariance between the depth prepass and the opaque pass
# that tests Equal against it
crowy_declare_spike(PrepassEqualSpike
    TEST render
SOURCES
    PrepassEqualSpike.cpp
)

# guards the standard pipeline's textures: a point-sampled palette, an atlas
# rect through the material's UV transform, an emissive map, and a surface
# that refuses the shadow it stands in
crowy_declare_spike(TexturedSpike
    TEST render
SOURCES
    TexturedSpike.cpp
)
