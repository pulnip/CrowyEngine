crowy_declare_module(UI)

target_link_libraries(CrowyUI
PUBLIC
    Crowy::CommonRHI
    Crowy::EngineCore
    imgui
PRIVATE
    Crowy::RHI
)

if(RENDER_BACKEND STREQUAL "Metal")
    target_compile_definitions(CrowyUI
    PRIVATE
        CROWY_METALRHI
    )
    target_link_libraries(CrowyUI
    PRIVATE
        Crowy::MetalRHI
    )
endif()