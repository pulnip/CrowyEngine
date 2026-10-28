#pragma once

// The cube for C++ and Slang: corner i's offset, edge i's two corners, and the
// most indices one case's triangles take.
#define MC_CORNER_OFFSETS                                                      \
    0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1
#define MC_EDGE_ENDS                                                           \
    0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7
#define MC_MAX_INDICES 16u
