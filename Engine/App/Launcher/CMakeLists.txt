add_executable(Crowy)
crowy_link_folder(Crowy asset)

target_sources(Crowy
PRIVATE
    main.cpp
)

target_link_libraries(Crowy
PRIVATE
    Crowy::App
    Crowy::Platform
)
