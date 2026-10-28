#pragma once

// The effects' numbers for C++ and Slang.

// the draw the first step staggers ages with; an effect's own draws keep to
// small k
#define CROWY_EFFECT_STAGGER_DRAW 0xA6Eu
// divides 2^32, so a step wrapped below 0 lands on the same phase
#define CROWY_EFFECT_LOOP_STEPS 4096u
// every effect kernel's threads a group
#define CROWY_EFFECT_GROUP_WIDTH 64u
