function(crowy_rhi_sample NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME}
        ${ARG_SOURCES}
    )

    target_link_options(${NAME}
    PRIVATE
        ${RENDER_BACKEND_LINK_OPTIONS}
    )
    target_link_libraries(${NAME}
    PUBLIC
        CrowyProjectInterface
    PRIVATE
        ${RENDER_BACKENDS}
        Crowy::Log
        ${ARG_DEPENDS}
    )
    if(WIN32)
        target_link_libraries(${NAME}
        PRIVATE
            delayimp.lib
        )
    endif()

    # launch-and-watch smoke test (see Tools/smoke_run.sh|.ps1);
    # registered here so every new sample gets one automatically.
    # run them with
    #   ctest --test-dir build -C Debug -L smoke
    # duration and frame capture come from CROWY_SMOKE_* environment
    # variables read by the scripts, so no values are baked in here.
    if(APPLE)
        set(SMOKE_COMMAND
            "${CMAKE_SOURCE_DIR}/Tools/smoke_run.sh"
            "$<TARGET_FILE:${NAME}>"
        )
    elseif(WIN32)
        set(SMOKE_COMMAND
            powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/Tools/smoke_run.ps1"
            "$<TARGET_FILE:${NAME}>"
        )
    endif()
    if(DEFINED SMOKE_COMMAND)
        add_test(
            NAME ${NAME}Smoke
            COMMAND ${SMOKE_COMMAND}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(${NAME}Smoke
        PROPERTIES
            LABELS "smoke"
            # windowed apps fight over focus, and the debug layer slows
            # everything down; one at a time keeps results stable
            RUN_SERIAL TRUE
            # generous: the watch window is env-configurable
            TIMEOUT 300
        )
    endif()
endfunction()

function(crowy_graphics_sample NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    crowy_rhi_sample(${NAME}
    SOURCES
        AppFramework.cpp
        ${ARG_SOURCES}
    DEPENDS
        Crowy::Platform
        ${ARG_DEPENDS}
    )
endfunction()

# ready-made scenes the samples can render
add_subdirectory(Scene)
# density field, noise and UI panel the two terrain samples share
add_subdirectory(Terrain)

crowy_graphics_sample(HelloWorld
SOURCES
    helloWorld.cpp
)

crowy_graphics_sample(HelloConstantBuffer
SOURCES
    helloConstantBuffer.cpp
)

crowy_graphics_sample(HelloTexture
SOURCES
    helloTexture.cpp
DEPENDS
    Crowy::Serialization
)

crowy_rhi_sample(HelloCompute
SOURCES
    helloCompute.cpp
)

crowy_graphics_sample(HelloIndirectDraw
SOURCES
    helloIndirectDraw.cpp
)

crowy_graphics_sample(CubeMapping
SOURCES
    CubeMapping.cpp
DEPENDS
    Crowy::Serialization
)

crowy_graphics_sample(NormalMapping
SOURCES
    NormalMapping.cpp
DEPENDS
    Crowy::Serialization
)

crowy_graphics_sample(CornellBoxSample
SOURCES
    CornellBoxSample.cpp
DEPENDS
    Crowy::SampleScene
)

crowy_graphics_sample(SkyboxFloorSample
SOURCES
    SkyboxFloorSample.cpp
DEPENDS
    Crowy::SampleScene
)

crowy_graphics_sample(Checkerboard
SOURCES
    Checkerboard.cpp
)

crowy_graphics_sample(BaseInstanceSpike
SOURCES
    BaseInstanceSpike.cpp
)

crowy_graphics_sample(TenThousandCubes
SOURCES
    TenThousandCubes.cpp
DEPENDS
    Crowy::Serialization
)

crowy_graphics_sample(BlackholeSimulation
SOURCES
    BlackholeSimulation.cpp
)

crowy_graphics_sample(UISample
SOURCES
    UISample.cpp
DEPENDS
    Crowy::UI
)

# headless; fails the smoke run if the CPU and GPU density fields drift apart
crowy_rhi_sample(TerrainDensityCheck
SOURCES
    TerrainDensityCheck.cpp
DEPENDS
    Crowy::SampleTerrain
)

# headless; runs the marching passes once and checks the mesh landed on the
# isosurface
crowy_rhi_sample(TerrainMarchCheck
SOURCES
    TerrainMarchCheck.cpp
DEPENDS
    Crowy::SampleTerrain
)

crowy_graphics_sample(SpanTerrainSample
SOURCES
    SpanTerrainSample.cpp
DEPENDS
    Crowy::SampleTerrain
    Crowy::UI
)

crowy_graphics_sample(MarchingTerrainSample
SOURCES
    MarchingTerrainSample.cpp
DEPENDS
    Crowy::SampleTerrain
    Crowy::UI
)
