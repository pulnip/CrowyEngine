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
