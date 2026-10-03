# a small island at night with the effects drawn over it; the render
# framework still lives with Render's samples, as the spikes compile it
crowy_declare_example(Island
SOURCES
    "${CMAKE_SOURCE_DIR}/Engine/Render/Sample/FlyCamera.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Render/Sample/RenderApp.cpp"
    Island.cpp
DEPENDS
    Crowy::Effects
    Crowy::Remote
    Crowy::Render
)

target_include_directories(Island
PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Render/Sample"
)
