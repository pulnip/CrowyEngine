crowy_declare_sample_library(Orbit
    TEST_DIRECTORY Test
    TEST_LABELS "unit"
SOURCES
    AsteroidBelt.cpp
    Kepler.cpp
    OrbitDrawArgs.cpp
    OrbitFill.cpp
    OrbitTrail.cpp
PUBLIC
    Crowy::Core
    Crowy::RHI
)
