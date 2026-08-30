crowy_declare_module(Serialization
PUBLIC
    Crowy::Core
    Crowy::RHI
PRIVATE
    ktx
    nlohmann_json::nlohmann_json
    stb
    ufbx
    Crowy::Log
)

crowy_declare_private_interface(Serialization)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Serialization
        DIRECTORY Test
        DEPENDS Crowy::Serialization::Private
        LABELS "unit"
    )

    add_subdirectory(Sample)
endif()
