# 1) Core Layer
add_subdirectory(Core)

# 2-0) Infrastructure Layer
add_subdirectory(Canvas2D)
add_subdirectory(Log)
add_subdirectory(Platform)
add_subdirectory(RHI)
add_subdirectory(Serialization)
# 2-1) Infrastructure Implement Layer
if(WIN32)
    add_subdirectory(DX12RHI)
elseif(APPLE)
    add_subdirectory(MetalRHI)
endif()
add_subdirectory(ImGuiCanvas)

add_subdirectory(UI)

add_subdirectory(Resource)
# 3-0) Subsystem Layer
# add_subdirectory(Render)
