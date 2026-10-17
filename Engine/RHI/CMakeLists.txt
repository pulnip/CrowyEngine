crowy_declare_interface(CommonRHI
    DIRECTORY Common
)

crowy_declare_module(RHI
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
PRIVATE
    Slang::Slang
    Crowy::Log
    OffsetAllocator
)

if(CROWY_ENABLE_TEST)
    # recording fakes the subsystem tests run against instead of a device
    crowy_declare_interface(RHITestSupport
        DIRECTORY TestSupport
        DEPENDS Crowy::RHI
    )

    crowy_declare_test(RHI
        DIRECTORY Test
        DEPENDS
            Crowy::RHI
            Crowy::RHITestSupport
        LABELS "unit"
    )

    add_subdirectory(Sample)
    add_subdirectory(Spike)
    add_subdirectory(Check)
endif()
