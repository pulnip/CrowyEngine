crowy_declare_module(Core)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Core
        DIRECTORY Test
        DEPENDS Crowy::Core
        LABELS "unit"
    )
endif()
