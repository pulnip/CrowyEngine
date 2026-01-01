# Layer 0
add_subdirectory(Log)
add_subdirectory(ECS)
add_subdirectory(EngineCore)

# Layer 1: Platform
add_subdirectory(Platform)
add_subdirectory(RHI)
# RHI Implementations
if(APPLE)
    add_subdirectory(MetalRHI)
elseif(WIN32)
    add_subdirectory(D3D12RHI)
else()
    # Null RHI as fallback
    set(USE_NULL_RHI ON)
endif()

# Layer 2: High-Level Systems
# add_subdirectory(Audio)
# add_subdirectory(Input)
# add_subdirectory(Physics)
add_subdirectory(Resource)
add_subdirectory(Render)
add_subdirectory(Scene)
# add_subdirectory(Script)

# Layer 3: Application
add_subdirectory(App)

if(CROWY_ENABLE_TEST)
    set(USE_NULL_RHI ON)
    # add_subdirectory(Test)
endif()

if(USE_NULL_RHI)
    add_subdirectory(NullRHI)
endif()