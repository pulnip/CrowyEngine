add_library(CrowyResource STATIC)

target_sources(CrowyResource
PRIVATE
    Private/dummy.cpp
)

target_include_directories(CrowyResource
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyResource
PUBLIC
    Crowy::Core
    Crowy::RHI
)

add_library(Crowy::Resource ALIAS CrowyResource)