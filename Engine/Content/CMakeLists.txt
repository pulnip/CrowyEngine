crowy_declare_module(Content
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
PRIVATE
    ktx
    stb
    ufbx
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Content
        DIRECTORY Test
        DEPENDS Crowy::Content
        LABELS "unit"
    )
    # fixtures the tests read in place, whatever the working directory
    target_compile_definitions(CrowyContentTest
    PRIVATE
        CROWY_CONTENT_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/Test/Data"
    )

    add_subdirectory(Check)
endif()
