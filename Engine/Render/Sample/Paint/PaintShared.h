#pragma once

// PaintLab's numbers, read alike by C++ and Slang: literals only.

// what the surface pass shows
#define PAINT_VIEW_LIT 0
#define PAINT_VIEW_ISLANDS 1
#define PAINT_VIEW_PAINT_ID 2
#define PAINT_VIEW_HEIGHT 3
#define PAINT_VIEW_DISTANCE 4
#define PAINT_VIEW_POSITION 5
#define PAINT_VIEW_EDGE_FADE 6
#define PAINT_VIEW_SCORE 7
#define PAINT_VIEW_DIVERGENCE 8
#define PAINT_VIEW_SIGNED_DISTANCE 9
#define PAINT_VIEW_COVERAGE 10
#define PAINT_VIEW_HEIGHT_FIELD 11
#define PAINT_VIEW_NORMAL 12
#define PAINT_VIEW_BASE_COLOR 13
#define PAINT_VIEW_ROUGHNESS 14
#define PAINT_VIEW_SHADING_MODEL 15

// what the atlas panel shows of the selected surface
#define PAINT_PANEL_ISLANDS 0
#define PAINT_PANEL_PAINT_ID 1
#define PAINT_PANEL_HEIGHT 2
#define PAINT_PANEL_DISTANCE 3
#define PAINT_PANEL_POSITION 4
#define PAINT_PANEL_EDGE_FADE 5

// the paint buffer's encoding: R = id + 8 gen, G height, B 1 - d / range
#define PAINT_ID_NONE 7
#define PAINT_DIST_RANGE 4.0f
// one full G in world cm: the shipped masters' displacement magnitude
#define PAINT_MAX_HEIGHT 9.0f
// a texel the bake found no surface for
#define PAINT_EMPTY_POSITION -64.0f

// a whole StampCustom rather than one stage of its build-up
#define PAINT_SHAPE_STAGE_FULL 8.0f
