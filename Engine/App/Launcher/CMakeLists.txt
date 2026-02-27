add_executable(Crowy)

set(INSTALL_TARGETS Crowy angelscript assimp)

if(TARGET SDL3-shared)
    list(APPEND INSTALL_TARGETS SDL3-shared)
elseif(TARGET SDL3-static)
    list(APPEND INSTALL_TARGETS SDL3-static)
endif()

install(TARGETS ${INSTALL_TARGETS}
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
    ARCHIVE DESTINATION . 
)

target_sources(Crowy
PRIVATE
    main.cpp
)

target_link_libraries(Crowy
PRIVATE
    Crowy::App
    Crowy::Platform
)
