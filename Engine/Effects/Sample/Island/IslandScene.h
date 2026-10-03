#pragma once

// The Island's numbers, read alike by C++ and Slang: literals only, each
// macro the components of one value.

// the island: an ellipsoid of these radii, its center this far under the sea
#define ISLAND_RADII 9.0f, 1.6f, 7.0f
#define ISLAND_CENTER_Y -1.2f

// toward the moon: 38 deg from +z toward +x, 20 deg up
#define ISLAND_TO_MOON 0.578533f, 0.342020f, 0.740488f
#define ISLAND_MOON_COLOR 0.55f, 0.65f, 1.0f
#define ISLAND_MOON_INTENSITY 0.45f
// the disc, wider than the real moon's 0.26 deg so it reads at 720 rows
#define ISLAND_MOON_DISC_RADIUS 0.0105f
#define ISLAND_MOON_DISC 3.0f, 2.9f, 2.7f

// the night sky overhead and at the horizon, and the far sea under it
#define ISLAND_SKY_ZENITH 0.004f, 0.006f, 0.016f
#define ISLAND_SKY_HORIZON 0.024f, 0.031f, 0.062f
#define ISLAND_FAR_SEA 0.010f, 0.014f, 0.030f

// radians a pixel spans at the frame's center: 2 tan 30 deg over 720 rows
#define ISLAND_PIXEL_ANGLE 0.00160375f

// the campfire on the island's crown, and its light inside the tripod
#define ISLAND_FIRE 0.0f, 0.4f, 0.0f
#define ISLAND_FIRE_LIGHT 0.0f, 0.65f, 0.0f
// the fire's breath on the world's loop: two ripples' cycles and depths
#define ISLAND_BREATH_SLOW 241u
#define ISLAND_BREATH_SLOW_DEPTH 0.12f
#define ISLAND_BREATH_FAST 854u
#define ISLAND_BREATH_FAST_DEPTH 0.06f

// the tipi around the fire: facets between poles from the first pole's
// azimuth on, the base's circumradius and height, the poles' crossing
#define ISLAND_TIPI_FACETS 10u
#define ISLAND_TIPI_FIRST_POLE 2.80998f
#define ISLAND_TIPI_RADIUS 1.8f
#define ISLAND_TIPI_BASE_Y 0.3f
#define ISLAND_TIPI_APEX_Y 4.1f
// the canvas stops under the smoke hole and leaves the first facets open
#define ISLAND_TIPI_CANVAS_TOP_Y 3.57f
#define ISLAND_TIPI_OPEN_FACETS 3u
