add_executable(CrowyEditor)

target_sources(CrowyEditor
PRIVATE
    main.cpp
)

target_link_libraries(CrowyEditor
PRIVATE
    Crowy::Platform
)