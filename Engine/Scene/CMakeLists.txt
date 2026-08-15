crowy_declare_module(Scene
PUBLIC
    Crowy::Core
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Scene
        DIRECTORY Test
        DEPENDS Crowy::Scene
        LABELS "unit"
    )
endif()
