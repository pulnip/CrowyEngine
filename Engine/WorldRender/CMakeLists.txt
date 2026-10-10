# The World's side of the renderer: the components that draw, and the
# systems that extract them into a RenderScene every frame. Render does not
# see the World; this module sees both, and registers from objects nothing
# references.
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
