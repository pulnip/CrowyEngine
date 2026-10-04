add_library(SamplePuzzle STATIC
    BuoyPop.cpp
    Puzzle.cpp
    PuzzlePins.cpp
    PuzzleSession.cpp
    PuzzleTrack.cpp
    SwingCut.cpp
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
target_compile_options(SamplePuzzle
PRIVATE
    ${CROWY_FP_CONTRACT_OFF}
)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SamplePuzzle
        DIRECTORY Test
        DEPENDS SamplePuzzle
        LABELS "unit"
    )
    target_compile_options(CrowySamplePuzzleTest
    PRIVATE
        ${CROWY_FP_CONTRACT_OFF}
    )
endif()
