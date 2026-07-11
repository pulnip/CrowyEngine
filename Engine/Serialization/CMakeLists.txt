smol_declare_module(Serialization)

target_compile_definitions(CrowySerialization
PRIVATE
    TOML_EXCEPTIONS=1
)

target_link_libraries(CrowySerialization
PUBLIC
    Crowy::Core
PRIVATE
    stb
    tomlplusplus::tomlplusplus
    Crowy::Log
)

smol_declare_private_interface(Serialization)

if(SMOL_ENABLE_TEST)
    smol_declare_test(Serialization
        DIRECTORY Test
        DEPENDS Crowy::Serialization::Private
        LABELS "unit"
    )

    add_subdirectory(Sample)
endif()
