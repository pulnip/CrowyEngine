add_library(CrowyApp STATIC)

target_sources(CrowyApp
PRIVATE
    Private/App.cpp
)

target_include_directories(CrowyApp
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyApp
PUBLIC
    Crowy::Core
PRIVATE
    Crowy::Platform
)

add_library(Crowy::Main ALIAS CrowyApp)