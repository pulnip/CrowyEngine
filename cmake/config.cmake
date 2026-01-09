set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/EHsc)
    add_compile_definitions(NOMINMAX)
endif()

include(cmake/deps.cmake)
include(cmake/sanitizer.cmake)
include(cmake/util.cmake)