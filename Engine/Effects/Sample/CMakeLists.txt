# a small island at night with the effects drawn over it
crowy_declare_example(Island
SOURCES
    Island.cpp
    IslandMeshes.cpp
DEPENDS
    Crowy::Effects
    SampleRenderApp
)

# the Island's rain and wind probes held against the CPU
crowy_declare_check(IslandCheck
SOURCES
    IslandCheck.cpp
DEPENDS
    Crowy::Effects
)
target_include_directories(IslandCheck
PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Effects/Check"
)
