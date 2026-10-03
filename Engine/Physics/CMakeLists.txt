crowy_declare_module(Physics
PUBLIC
    Crowy::Core
PRIVATE
    Crowy::Log
    Jolt
)

# no fused multiply-add, so Windows and the Mac round alike
set(CROWY_FP_CONTRACT_OFF
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:${MSVC}>>:/clang:-ffp-contract=off>
    $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang>,$<NOT:$<BOOL:${MSVC}>>>:-ffp-contract=off>
)
target_compile_options(CrowyPhysics
PRIVATE
    ${CROWY_FP_CONTRACT_OFF}
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Physics
        DIRECTORY Test
        DEPENDS Crowy::Physics
        LABELS "unit"
    )
    # test inputs feed the simulation too, and arm64 contracts by default
    target_compile_options(CrowyPhysicsTest
    PRIVATE
        ${CROWY_FP_CONTRACT_OFF}
    )

    # puzzles over the public API, run by tests and by the playground
    add_subdirectory(Sample/Puzzle)
endif()
