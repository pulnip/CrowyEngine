smol_declare_interface(CommonRHI
    DIRECTORY Common
)

smol_declare_module(RHI)

target_include_directories(CrowyRHI
PRIVATE
    ${slang_bin_SOURCE_DIR}/include
)

target_link_libraries(CrowyRHI
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
PRIVATE
    ${slang_bin_SOURCE_DIR}/lib/slang.lib
    Crowy::Log
)

add_custom_command(TARGET CrowyRHI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${slang_bin_SOURCE_DIR}/bin/slang.dll"
        "${slang_bin_SOURCE_DIR}/bin/slang-compiler.dll"
        "${CMAKE_BINARY_DIR}/bin"
    COMMENT "Copying slang.dll"
)

if(SMOL_ENABLE_TEST)
    add_subdirectory(Sample)
endif()
