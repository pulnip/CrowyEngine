#include "Puzzle.hpp"

#include "Assert.hpp"
#include "PuzzleContent.hpp"

namespace Crowy
{
    Puzzle makePuzzle(PuzzleKind kind) {
        using enum PuzzleKind;

        switch(kind) {
        case PlateGate:
            return makePlateGate();
        case ToppleBridge:
            return makeToppleBridge();
        case SwingDoor:
            return makeSwingDoor();
        }
        CROWY_ASSERT(false, "no such puzzle");
        return makePlateGate();
    }

    const PuzzleInputs& inputsOf(const Puzzle& puzzle, PuzzleMode mode) {
        return mode == PuzzleMode::Solution ? puzzle.solution : puzzle.control;
    }
}
