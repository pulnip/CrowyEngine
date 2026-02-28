set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(CLANG_CL "clang-cl")
if(CLANG_CL)
    set(CMAKE_C_COMPILER   "${CLANG_CL}")
    set(CMAKE_CXX_COMPILER "${CLANG_CL}")
endif()

find_program(LLD_LINK "lld-link")
if(LLD_LINK)
    set(CMAKE_LINKER "${LLD_LINK}")
endif()

set(CMAKE_C_COMPILER_TARGET   "x86_64-pc-windows-msvc")
set(CMAKE_CXX_COMPILER_TARGET "x86_64-pc-windows-msvc")