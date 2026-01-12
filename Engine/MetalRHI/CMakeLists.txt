crowy_declare_module(MetalRHI)

target_include_directories(CrowyMetalRHI
SYSTEM PRIVATE
    "${metal-cpp_SOURCE_DIR}"
)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyMetalRHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyMetalRHI PUBLIC Crowy::RHI)
endif()

target_link_libraries(CrowyMetalRHI
PUBLIC
    Crowy::CommonRHI
PRIVATE
    ${METAL_LIBRARY}
    ${METALKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${FOUNDATION_LIBRARY}
)
