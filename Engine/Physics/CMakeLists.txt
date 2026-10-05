crowy_declare_module(Physics
PUBLIC
    Crowy::Core
PRIVATE
    Crowy::Log
    Jolt
)

crowy_fp_contract_off(CrowyPhysics)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(Physics
        DIRECTORY Test
        DEPENDS Crowy::Physics
        LABELS "unit"
    )
    # test inputs feed the simulation too, and arm64 contracts by default
    crowy_fp_contract_off(CrowyPhysicsTest)

    # puzzles over the public API, run by tests and by the playground
    add_subdirectory(Sample/Puzzle)
endif()
