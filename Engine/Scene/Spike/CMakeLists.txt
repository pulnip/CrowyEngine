# guards the imported set as a scene: textures through material files, an
# environment actor and the first of several cameras, in one picture
crowy_declare_spike(BacklotSceneSpike
    TEST render
SOURCES
    BacklotSceneSpike.cpp
DEPENDS
    Crowy::Scene
    Crowy::WorldRender
    Crowy::SampleRenderApp
)
