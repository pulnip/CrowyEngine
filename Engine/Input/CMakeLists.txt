add_library(CrowyInput STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyInput
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyInput
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyInput
PUBLIC
    Crowy::EngineCore
)

add_library(Crowy::Input ALIAS CrowyInput)

if(CROWY_ENABLE_TEST)
    add_subdirectory(Test)
endif()