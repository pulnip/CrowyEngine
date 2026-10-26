# A physics world written one way into render primitives. It knows no
# sample, so it can move out of the samples as it is.
crowy_declare_sample_library(PhysicsSync
    # its tests repeat the library's arithmetic and compare bit for bit
    FP_CONTRACT_OFF
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    BodyPrimitiveSync.cpp
PUBLIC
    Crowy::Core
    Crowy::Physics
    Crowy::Render
)
