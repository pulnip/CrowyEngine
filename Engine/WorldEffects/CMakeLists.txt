crowy_declare_module(WorldEffects
WHOLE_ARCHIVE
PUBLIC
    Crowy::Effects
    Crowy::World
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(WorldEffects
        DIRECTORY Test
        DEPENDS
            Crowy::WorldEffects
            Crowy::RHITestSupport
        LABELS "unit"
    )
endif()
