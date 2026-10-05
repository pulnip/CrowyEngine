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
    # fixtures the tests read in place, whatever the working directory
    target_compile_definitions(CrowySerializationTest
    PRIVATE
        CROWY_SERIALIZATION_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/Test/Data"
    )

    add_subdirectory(Sample)
    add_subdirectory(Check)
endif()
