function(smol_dx12_sample NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME}Sample
        Sample.cpp
        ${ARG_SOURCES}
    )

    target_link_options(${NAME}Sample
    PRIVATE
        ${RENDER_BACKEND_LINK_OPTIONS}
    )
    target_link_libraries(${NAME}Sample
    PRIVATE
        Crowy::Log
        # use SDL Window
        SDL3::SDL3
        Crowy::Platform::Private
        # DirectX 12 related lib
        delayimp.lib
        Crowy::DX12RHI::Private
        ${ARG_DEPENDS}
    )
endfunction()

smol_dx12_sample(DX12Triangle
SOURCES
    triangle.cpp
)

smol_dx12_sample(DX12HelloConstantBuffer
SOURCES
    helloConstantBuffer.cpp
)

smol_dx12_sample(DX12HelloTexture
SOURCES
    helloTexture.cpp
DEPENDS
    Crowy::Serialization
)
