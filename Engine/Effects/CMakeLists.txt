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
        DEPENDS
            Crowy::Effects
            Crowy::RHITestSupport
        LABELS "unit"
    )

    add_subdirectory(Check)
    add_subdirectory(Sample)
endif()
