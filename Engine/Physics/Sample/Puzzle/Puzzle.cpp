#include "Puzzle.hpp"

#include "Assert.hpp"
#include "PuzzleContent.hpp"

namespace Crowy
{
    Puzzle makePuzzle(PuzzleKind kind) {
        using enum PuzzleKind;

        switch(kind) {
        case SwingCut:
            return makeSwingCut();
        }
        CROWY_ASSERT(false, "no such puzzle");
        return makeSwingCut();
    }

    const PuzzleScript& scriptOf(const Puzzle& puzzle, PuzzleMode mode) {
        return mode == PuzzleMode::Solution ? puzzle.solution : puzzle.control;
    }
}
