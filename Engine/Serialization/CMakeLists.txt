crowy_declare_module(Serialization
PUBLIC
    Crowy::Core
PRIVATE
    nlohmann_json::nlohmann_json
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Serialization
        DIRECTORY Test
        DEPENDS Crowy::Serialization
        LABELS "unit"
    )
endif()
