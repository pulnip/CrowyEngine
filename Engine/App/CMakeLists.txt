# No Crowy::World, deliberately: the host draws a RenderScene and knows no
# World. An executable that runs a FrameHost names Crowy::App itself.
crowy_declare_module(App
PUBLIC
    Crowy::Asset
    Crowy::Core
    Crowy::Platform
    Crowy::Reflection
    Crowy::Remote
    Crowy::Render
    Crowy::RHI
    Crowy::Serialization
    Crowy::UI
PRIVATE
    Crowy::Log
)
