function(crowy_serialization_sample NAME)
    cmake_parse_arguments(ARG "" "SOURCES" "DEPENDS" ${ARGN})

    add_executable(Crowy${NAME}Sample ${ARG_SOURCES})
    crowy_copy_runtime_files(Crowy${NAME}Sample)

    target_link_libraries(Crowy${NAME}Sample
    PRIVATE
        Crowy::Serialization
        ${ARG_DEPENDS}
    )
endfunction()

crowy_serialization_sample(ImageLoader
    SOURCES ImageLoaderSample.cpp
)
