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
