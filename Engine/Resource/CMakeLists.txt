crowy_declare_module(Resource)

target_compile_definitions(CrowyResource
PRIVATE
    TOML_EXCEPTIONS=1
)

target_include_directories(CrowyResource
SYSTEM PRIVATE
    "${stb_SOURCE_DIR}"
)

target_link_libraries(CrowyResource
PUBLIC
    Crowy::CommonRHI
    Crowy::Core
    Crowy::EngineCore
PRIVATE
    assimp::assimp
    tomlplusplus::tomlplusplus
    Crowy::Log
    Crowy::RHI
)

crowy_rhi_macro(Resource)
crowy_declare_private_interface(Resource)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Resource
        DIRECTORY Test
        DEPENDS CrowyResource_Private
                Crowy::Log
                Crowy::NullRHI
        LABELS "unit"
    )
endif()