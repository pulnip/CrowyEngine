crowy_declare_module(Reflection
PUBLIC
    Crowy::Core
    Crowy::Serialization
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Reflection
        DIRECTORY Test
        DEPENDS "$<LINK_LIBRARY:WHOLE_ARCHIVE,Crowy::Reflection>"
        LABELS "unit"
    )
endif()
