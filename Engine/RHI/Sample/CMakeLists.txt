function(crowy_rhi_sample NAME)
    cmake_parse_arguments(ARG "" "SOURCES" "" ${ARGN})

    add_executable(Crowy${NAME} ${ARG_SOURCES})
    crowy_rhi_macro(${NAME})

    target_link_libraries(Crowy${NAME}
    PRIVATE
        SDL3::SDL3
        Crowy::Log
        Crowy::Resource
    )

    if(RENDER_BACKEND STREQUAL "Metal")
        target_link_libraries(Crowy${NAME}
        PRIVATE
            Crowy::MetalRHI
        )
    elseif(RENDER_BACKEND STREQUAL "D3D11")
        target_link_libraries(Crowy${NAME}
        PRIVATE
            Crowy::D3D11RHI
        )
    elseif(RENDER_BACKEND STREQUAL "D3D12")
        target_link_libraries(Crowy${NAME}
        PRIVATE
            Crowy::D3D12RHI
        )
    else()
        target_link_libraries(Crowy${NAME}
        PRIVATE
            Crowy::NullRHI
        )
    endif()
endfunction()

crowy_rhi_sample(RHISample
    SOURCES Sample.cpp
)

crowy_rhi_sample(VectorAddition
    SOURCES VectorAddition.cpp
)