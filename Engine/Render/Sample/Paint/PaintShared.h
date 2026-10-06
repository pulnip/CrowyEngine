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
// G along one atlas line, as a graph
#define PAINT_PANEL_PROFILE 6

// the paint buffer's encoding: R = id + 8 gen, G height, B 1 - d / range
#define PAINT_ID_NONE 7
// one full G in world cm: the shipped masters' displacement magnitude
#define PAINT_MAX_HEIGHT 9.0f

// how the edge between paint and not is read
#define PAINT_EDGE_NEAREST 0u
#define PAINT_EDGE_BILINEAR 1u
#define PAINT_EDGE_SIGNED_DISTANCE 2u

// how teams are laid over each other
#define PAINT_BLEND_NAIVE 0u
#define PAINT_BLEND_CONSUMED 1u

// how G is read back
#define PAINT_HEIGHT_NEAREST 0u
#define PAINT_HEIGHT_BILINEAR 1u
#define PAINT_HEIGHT_BSPLINE 2u

// the lobes a debug toggle can take out
#define PAINT_LOBE_DIFFUSE 1u
#define PAINT_LOBE_SPECULAR 2u
#define PAINT_LOBE_HAZE 4u
#define PAINT_LOBE_FUZZ 8u
#define PAINT_LOBE_SSS 16u
#define PAINT_LOBE_COAT 32u
#define PAINT_LOBE_SKY 64u

// a whole StampCustom rather than one stage of its build-up
#define PAINT_SHAPE_STAGE_FULL 8.0f
