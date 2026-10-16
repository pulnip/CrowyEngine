crowy_declare_module(Asset
PUBLIC
    Crowy::Core
    Crowy::Reflection
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Asset
        DIRECTORY Test
        DEPENDS Crowy::Asset
        LABELS "unit"
    )
endif()
