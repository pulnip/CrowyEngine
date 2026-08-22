# No Crowy::Scene and no gameplay module, deliberately:
# a transform arrives here as a Mat4 and a bounding box.
# Extraction lives with its caller.
crowy_declare_module(Render
PUBLIC
    Crowy::Core
    Crowy::RHI
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Render
        DIRECTORY Test
        DEPENDS Crowy::Render
        LABELS "unit"
    )

    add_subdirectory(Sample)
endif()
