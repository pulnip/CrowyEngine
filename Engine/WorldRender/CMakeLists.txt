# Render does not see the World; this module sees both, and registers from
# objects nothing references.
crowy_declare_module(WorldRender
WHOLE_ARCHIVE
PUBLIC
    Crowy::Render
    Crowy::World
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(WorldRender
        DIRECTORY Test
        DEPENDS Crowy::WorldRender
        LABELS "unit"
    )
endif()
