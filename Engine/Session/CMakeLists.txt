# A World with the render side it draws into, built and destroyed as one.
# Knows no App and no Editor: the host draws a session's scene.
crowy_declare_module(Session
PUBLIC
    Crowy::Core
    Crowy::Render
    Crowy::Scene
    Crowy::World
    Crowy::WorldEffects
    Crowy::WorldRender
PRIVATE
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Session
        DIRECTORY Test
        DEPENDS
            Crowy::Session
            Crowy::RHITestSupport
        LABELS "unit"
    )
    # the engine: mount the built-in materials resolve against
    target_compile_definitions(CrowySessionTest
    PRIVATE
        CROWY_ENGINE_ROOT="${PROJECT_SOURCE_DIR}/Engine"
    )
endif()
