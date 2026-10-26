crowy_declare_sample_library(Puzzle
    # the literals and the tracks' poses enter the simulation
    FP_CONTRACT_OFF
    TEST_DIRECTORY Test
    TEST_LABELS "scenario"
SOURCES
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
PUBLIC
    Crowy::Physics
)
