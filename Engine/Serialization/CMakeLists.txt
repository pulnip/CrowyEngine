crowy_declare_module(Serialization
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
PRIVATE
    ktx
    nlohmann_json::nlohmann_json
    stb
    ufbx
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Serialization
        DIRECTORY Test
        DEPENDS Crowy::Serialization
        LABELS "unit"
    )
endif()
