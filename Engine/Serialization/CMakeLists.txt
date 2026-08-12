crowy_declare_module(Serialization
PUBLIC
    Crowy::Core
    Crowy::RHI
PRIVATE
    ktx
    stb
    tomlplusplus::tomlplusplus
    ufbx
    Crowy::Log
)

target_compile_definitions(CrowySerialization
PRIVATE
    TOML_EXCEPTIONS=1
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
