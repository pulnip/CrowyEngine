crowy_declare_interface(CommonRHI
    DIRECTORY Common
)

crowy_declare_module(RHI)

target_link_libraries(CrowyRHI
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
PRIVATE
    Slang::Slang
    Crowy::Log
    OffsetAllocator
)

if(SLANG_RUNTIME_LIBS)
    add_custom_command(TARGET CrowyRHI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SLANG_RUNTIME_LIBS}
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMENT "Copying slang runtime libraries into ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        VERBATIM
    )
endif()

if(CROWY_ENABLE_TEST)
    add_subdirectory(Sample)
endif()
