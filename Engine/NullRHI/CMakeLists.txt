crowy_declare_module(NullRHI)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyNullRHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyNullRHI PUBLIC Crowy::RHI)
endif()

target_link_libraries(CrowyNullRHI
PUBLIC
    Crowy::CommonRHI
)
