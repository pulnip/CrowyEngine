# a small island at night with the effects drawn over it
crowy_declare_example(Island
    TEST app
SOURCES
    Island.cpp
    IslandMeshes.cpp
DEPENDS
    Crowy::Effects
    Crowy::WorldEffects
    Crowy::WorldRender
)

# the Island's rain and wind probes held against the CPU
crowy_declare_check(IslandCheck
    TEST gpu
SOURCES
    IslandCheck.cpp
DEPENDS
    Crowy::Effects
)
target_include_directories(IslandCheck
PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Effects/Check"
)
