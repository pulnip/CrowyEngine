# listens and answers until told to quit, so it opens no window
crowy_declare_example(HelloCommandPort
    HEADLESS
SOURCES
    HelloCommandPort.cpp
DEPENDS
    Crowy::Remote
)
