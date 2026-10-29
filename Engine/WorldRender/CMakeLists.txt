# Render does not see the World; this module sees both, and registers from
# objects nothing references.
crowy_declare_module(WorldRender
WHOLE_ARCHIVE
PUBLIC
    Crowy::Render
    Crowy::World
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(WorldRender
        DIRECTORY Test
        DEPENDS
            Crowy::Scene
            Crowy::WorldRender
        LABELS "unit"
    )
    # the sample's scene file, which the round-trip test reads in place
    target_compile_definitions(CrowyWorldRenderTest
    PRIVATE
        CROWY_HELLO_SCENE_FILE="${PROJECT_SOURCE_DIR}/Engine/Scene/Sample/HelloScene.json"
    )
endif()
