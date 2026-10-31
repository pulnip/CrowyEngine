# a scene file opened and drawn, with no content made by code; the one
# windowed run of MeshComponent and of material files
crowy_declare_example(HelloScene
    TEST app
SOURCES
    HelloScene.cpp
DEPENDS
    Crowy::Scene
    Crowy::WorldRender
)

add_subdirectory(Backlot)
