crowy_declare_module(Input)

target_link_libraries(CrowyInput
PUBLIC
    Crowy::EngineCore
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Input
        DIRECTORY Test
        DEPENDS Crowy::Input
        LABELS "unit"
    )
endif()