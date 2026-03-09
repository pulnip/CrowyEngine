function(crowy_render_sample NAME)
    cmake_parse_arguments(ARG "" "SOURCES" "" ${ARGN})

    add_executable(Crowy${NAME} ${ARG_SOURCES})
    crowy_rhi_macro(${NAME})

    target_link_libraries(Crowy${NAME}
    PRIVATE
        SDL3::SDL3
        Crowy::Log
        Crowy::Render
        # Crowy::Resource
        Crowy::RHI
        Crowy::UI
    )
endfunction()

crowy_render_sample(RenderSample
    SOURCES Sample.cpp
)
target_link_libraries(CrowyRenderSample
PRIVATE
    Crowy::Resource
)

crowy_render_sample(BlackHole
    SOURCES BlackHole.cpp
)
