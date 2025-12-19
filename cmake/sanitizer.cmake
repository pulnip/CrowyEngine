add_library(sanitizer INTERFACE)
target_compile_options(sanitizer
INTERFACE
    -fsanitize=address,leak -fno-omit-frame-pointer
)
target_link_options(sanitizer
INTERFACE
    -fsanitize=address
)

function(rt_apply_sanitizer target)
    if(RENDERTOY_ENABLE_ASAN AND NOT MSVC)
        target_link_libraries(${target} PRIVATE sanitizer)
    endif()
endfunction()