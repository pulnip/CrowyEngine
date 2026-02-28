execute_process(
    COMMAND brew --prefix llvm
    OUTPUT_VARIABLE BREW_LLVM_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE BREW_RESULT
    ERROR_QUIET
)

if(BREW_RESULT EQUAL 0 AND EXISTS "${BREW_LLVM_PREFIX}")
    set(CMAKE_C_COMPILER   "${BREW_LLVM_PREFIX}/bin/clang")
    set(CMAKE_CXX_COMPILER "${BREW_LLVM_PREFIX}/bin/clang++")
    set(CMAKE_AR           "${BREW_LLVM_PREFIX}/bin/llvm-ar")
    set(CMAKE_RANLIB       "${BREW_LLVM_PREFIX}/bin/llvm-ranlib")

    find_program(LLD_FOUND "ld.lld" HINTS "${BREW_LLVM_PREFIX}/bin")
    if(LLD_FOUND)
        set(CMAKE_EXE_LINKER_FLAGS_INIT    "-fuse-ld=lld -L${BREW_LLVM_PREFIX}/lib/c++")
        set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld -L${BREW_LLVM_PREFIX}/lib/c++")
        set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld -L${BREW_LLVM_PREFIX}/lib/c++")
    endif()
endif()