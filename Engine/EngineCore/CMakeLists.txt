add_library(CrowyEngineCore STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyEngineCore
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyEngineCore
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyEngineCore
PUBLIC
    Crowy::Core
    Crowy::CommonRHI
)

add_library(Crowy::EngineCore ALIAS CrowyEngineCore)