crowy_declare_module(Script)

target_sources(CrowyScript
PRIVATE
    ${angelscript_SOURCE_DIR}/add_on/scriptarray/scriptarray.cpp
    ${angelscript_SOURCE_DIR}/add_on/scriptmath/scriptmath.cpp
    ${angelscript_SOURCE_DIR}/add_on/scriptmath/scriptmathcomplex.cpp
    ${angelscript_SOURCE_DIR}/add_on/scriptstdstring/scriptstdstring.cpp
)

target_include_directories(CrowyScript
SYSTEM PRIVATE
    ${angelscript_SOURCE_DIR}/add_on/scriptarray
    ${angelscript_SOURCE_DIR}/add_on/scriptmath
    ${angelscript_SOURCE_DIR}/add_on/scriptstdstring
)

target_link_libraries(CrowyScript
PUBLIC
    Crowy::Core
    Crowy::EngineCore
    Crowy::ECS
    Crowy::Input
    Crowy::Resource
PRIVATE
    angelscript
    Crowy::Log
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Script
        DIRECTORY Test
        DEPENDS Crowy::Script
        LABELS "unit"
    )
endif()