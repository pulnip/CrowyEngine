# No Crowy::World, deliberately:
# a transform arrives here as a Mat4 and a bounding box.
# Extraction lives with its caller.
crowy_declare_module(Render
PUBLIC
    Crowy::Asset
    Crowy::Core
    Crowy::RHI
    Crowy::Serialization
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Render
        DIRECTORY Test
        DEPENDS
            Crowy::Render
            Crowy::RHITestSupport
        LABELS "unit"
    )
    # a model file the mesh asset tests load in place
    target_compile_definitions(CrowyRenderTest
    PRIVATE
        CROWY_MODEL_TEST_DATA="${PROJECT_SOURCE_DIR}/Engine/Serialization/Test/Data"
    )

    add_subdirectory(Sample)
    add_subdirectory(Spike)
endif()
