function(smol_rhi_sample NAME)
    cmake_parse_arguments(ARG "" "SOURCES" "DEPENDS" ${ARGN})

    add_executable(${NAME}Sample ${ARG_SOURCES})

    target_link_options(${NAME}Sample
    PRIVATE
        ${RENDER_BACKEND_LINK_OPTIONS}
    )
    target_link_libraries(${NAME}Sample
    PRIVATE
        Crowy::Platform
        Crowy::Log
        ${RENDER_BACKENDS}
        ${ARG_DEPENDS}
    )
endfunction()

smol_rhi_sample(Texture
SOURCES TextureSample.cpp
DEPENDS
    Crowy::Serialization
    SDL3::SDL3
    Crowy::Platform::Private
)
