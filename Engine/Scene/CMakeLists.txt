crowy_declare_module(Scene)

target_link_libraries(CrowyScene
PUBLIC
    Crowy::Core
    Crowy::EngineCore
    Crowy::ECS
    Crowy::Resource
PRIVATE
    Crowy::Log
)

crowy_declare_private_interface(Scene)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Scene
        DIRECTORY Test
        DEPENDS CrowyScene_Private
                Crowy::NullRHI
        LABELS "unit"
    )
endif()