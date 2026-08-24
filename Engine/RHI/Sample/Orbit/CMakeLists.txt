add_library(SampleOrbit STATIC
    AsteroidBelt.cpp
    Kepler.cpp
    OrbitDrawArgs.cpp
    OrbitFill.cpp
    OrbitTrail.cpp
)

target_include_directories(SampleOrbit
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SampleOrbit
PUBLIC
    Crowy::Core
    # OrbitTrail.hpp hands out command list and buffer types
    Crowy::RHI
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SampleOrbit
        DIRECTORY Test
        DEPENDS SampleOrbit
        LABELS "unit"
    )
endif()
