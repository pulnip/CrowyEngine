function(smol_rhi_sample NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME}
        AppFramework.cpp
        ${ARG_SOURCES}
    )

    target_link_options(${NAME}
    PRIVATE
        ${RENDER_BACKEND_LINK_OPTIONS}
    )
    target_link_libraries(${NAME}
    PRIVATE
        Crowy::Log
        # use SDL Window
        SDL3::SDL3
        Crowy::Platform::Private
        ${RENDER_BACKENDS}
        ${ARG_DEPENDS}
    )
    if(WIN32)
        target_link_libraries(${NAME}
        PRIVATE
            delayimp.lib
        )
    endif()
endfunction()

smol_rhi_sample(HelloWorld
SOURCES
    helloWorld.cpp
)

smol_rhi_sample(HelloConstantBuffer
SOURCES
    helloConstantBuffer.cpp
)

smol_rhi_sample(HelloTexture
SOURCES
    helloTexture.cpp
DEPENDS
    Crowy::Serialization
)
