add_library(CrowyNullRHI STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyNullRHI
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyNullRHI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyNullRHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyNullRHI PUBLIC Crowy::RHI)
endif()

target_link_libraries(CrowyNullRHI
PUBLIC
    Crowy::CommonRHI
)

add_library(Crowy::NullRHI ALIAS CrowyNullRHI)