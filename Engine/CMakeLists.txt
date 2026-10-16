# 1) Core Layer
add_subdirectory(Core)

# 2-0) Interface Layer
add_subdirectory(RHI)
# 2-1) Infrastructure Layer
add_subdirectory(Log)
add_subdirectory(Platform)
add_subdirectory(Reflection)
add_subdirectory(Serialization)
add_subdirectory(Remote)
# 2-2) Infrastructure Implement Layer
if(WIN32)
    add_subdirectory(DX12RHI)
elseif(APPLE)
    add_subdirectory(MetalRHI)
endif()

add_subdirectory(UI)

# 3-0) Subsystem Layer
add_subdirectory(World)
if(CROWY_ENABLE_PHYSICS)
    add_subdirectory(Physics)
endif()
add_subdirectory(Render)
# 3-1) on top of Render
add_subdirectory(Effects)
# 3-2) the World's side of each subsystem
add_subdirectory(WorldRender)
add_subdirectory(WorldEffects)
# 3-3) the World saved as a scene file
add_subdirectory(Scene)
