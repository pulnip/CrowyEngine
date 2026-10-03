# GPU fields, the kernels that step them, and the effects drawn from them;
# CPU parameters go in, nothing is read back
crowy_declare_module(Effects
PUBLIC
    Crowy::Core
    Crowy::RHI
    Crowy::Render
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Effects
        DIRECTORY Test
        DEPENDS Crowy::Effects
        LABELS "unit"
    )
    # Render's FakeDevice and FakeCommandList
    target_include_directories(CrowyEffectsTest
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Engine/Render/Test"
    )

    add_subdirectory(Check)
endif()
