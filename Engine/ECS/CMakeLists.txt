crowy_declare_module(ECS)

target_link_libraries(CrowyECS
PUBLIC
    Crowy::Core
    # NOTE: ARCHETYPES Definitions
    Crowy::EngineCore
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(ECS
        DIRECTORY Test
        DEPENDS Crowy::ECS
        LABELS "unit"
    )
endif()