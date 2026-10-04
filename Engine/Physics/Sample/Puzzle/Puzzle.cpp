#include "Puzzle.hpp"

#include "Assert.hpp"
#include "PuzzleContent.hpp"

namespace Crowy
{
    Puzzle makePuzzle(PuzzleKind kind) {
        using enum PuzzleKind;

        switch(kind) {
        case KickerJump:
            return makeKickerJump();
        case TiltTray:
            return makeTiltTray();
        case CarriedScoop:
            return makeCarriedScoop();
        case SwingCut:
            return makeSwingCut();
        case LeverCatapult:
            return makeLeverCatapult();
        case BuoyPop:
            return makeBuoyPop();
        }
        CROWY_ASSERT(false, "no such puzzle");
        return makeKickerJump();
    }

    const PuzzleScript& scriptOf(const Puzzle& puzzle, PuzzleMode mode) {
        return mode == PuzzleMode::Solution ? puzzle.solution : puzzle.control;
    }
}
