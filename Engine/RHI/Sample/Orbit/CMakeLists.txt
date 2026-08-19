add_library(SampleOrbit STATIC
    Kepler.cpp
)

target_include_directories(SampleOrbit
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SampleOrbit
PUBLIC
    Crowy::Core
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SampleOrbit
        DIRECTORY Test
        DEPENDS SampleOrbit
        LABELS "unit"
    )
endif()
