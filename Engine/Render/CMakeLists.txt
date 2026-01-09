add_library(CrowyRender STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyRender
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyRender
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyRender
PUBLIC
    Crowy::CommonRHI
    Crowy::EngineCore
PRIVATE
    Crowy::Log
    Crowy::RHI
)

add_library(Crowy::Render ALIAS CrowyRender)
