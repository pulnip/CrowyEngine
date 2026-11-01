# opens a scene file for editing in a Scene View, in memory
crowy_declare_runtime(Editor
SOURCES
    Editor.cpp
DEPENDS
    Crowy::App
    Crowy::Editor
    Crowy::Session
)
