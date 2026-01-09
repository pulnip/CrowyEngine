add_executable(Crowy)

set_target_properties(Crowy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Game"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Game"
)

target_sources(Crowy
PRIVATE
    main.cpp
)

target_link_libraries(Crowy
PRIVATE
    Crowy::App
    Crowy::Platform
)
