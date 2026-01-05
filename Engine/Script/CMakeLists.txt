add_library(CrowyScript STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyScript
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${angelscript_SOURCE_DIR}/add_on/scriptarray/scriptarray.cpp
    ${angelscript_SOURCE_DIR}/add_on/scriptmath/scriptmath.cpp
    ${angelscript_SOURCE_DIR}/add_on/scriptmath/scriptmathcomplex.cpp
    ${angelscript_SOURCE_DIR}/add_on/scriptstdstring/scriptstdstring.cpp
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyScript
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
PRIVATE
    ${angelscript_SOURCE_DIR}/add_on/scriptarray
    ${angelscript_SOURCE_DIR}/add_on/scriptmath
    ${angelscript_SOURCE_DIR}/add_on/scriptstdstring
)

target_link_libraries(CrowyScript
PUBLIC
    Crowy::Core
    Crowy::EngineCore
    Crowy::Resource
PRIVATE
    angelscript
    Crowy::Log
)

add_library(Crowy::Script ALIAS CrowyScript)
