crowy_declare_module(MetalRHI)

target_include_directories(CrowyMetalRHI
SYSTEM PRIVATE
    "${metal-cpp_SOURCE_DIR}"
)

target_link_libraries(CrowyMetalRHI
PUBLIC
    Crowy::CommonRHI
    Crowy::RHI
PRIVATE
    ${METAL_LIBRARY}
    ${METALKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${FOUNDATION_LIBRARY}
    SDL3::SDL3
)
