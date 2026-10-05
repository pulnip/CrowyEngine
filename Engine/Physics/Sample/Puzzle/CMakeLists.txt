add_library(SamplePuzzle STATIC
    BuoyPop.cpp
    CarriedScoop.cpp
    KickerJump.cpp
    LeverCatapult.cpp
    Puzzle.cpp
    PuzzlePins.cpp
    PuzzleSession.cpp
    PuzzleTrack.cpp
    SwingCut.cpp
    TiltTray.cpp
)

target_include_directories(SamplePuzzle
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SamplePuzzle
PUBLIC
    Crowy::Physics
)

# the literals and the tracks' poses enter the simulation
crowy_fp_contract_off(SamplePuzzle)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SamplePuzzle
        DIRECTORY Test
        DEPENDS SamplePuzzle
        LABELS "unit"
    )
    crowy_fp_contract_off(CrowySamplePuzzleTest)
endif()
