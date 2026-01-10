set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# sanitizer config
if(MSVC)
    add_compile_options(
        $<$<CONFIG:Debug>:/fsanitize=address>
    )
else()
    add_compile_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    add_link_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

include(deps)
include(util)

if(CROWY_ENABLE_TEST)
    enable_testing()
    include(CTest)
    include(GoogleTest)
endif()