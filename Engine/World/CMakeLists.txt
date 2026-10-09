crowy_declare_module(World
PUBLIC
    Crowy::Core
    Crowy::Reflection
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(World
        DIRECTORY Test
        DEPENDS Crowy::World
        LABELS "unit"
    )
endif()
