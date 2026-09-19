# The debug command port: HTTP/JSON on 127.0.0.1, drained from the frame.
crowy_declare_module(Remote
PUBLIC
    Crowy::Core
    Crowy::Reflection
    Crowy::Serialization
PRIVATE
    Crowy::Log
)

if(WIN32)
    target_link_libraries(CrowyRemote
    PRIVATE
        ws2_32
    )
endif()

crowy_declare_private_interface(Remote)

if(CROWY_ENABLE_TEST)
    add_subdirectory(Sample)
endif()
