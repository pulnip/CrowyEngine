crowy_declare_module(Scene
PUBLIC
    Crowy::Serialization
    Crowy::World
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Scene
        DIRECTORY Test
        DEPENDS
            Crowy::Scene
        LABELS "unit"
    )
endif()
