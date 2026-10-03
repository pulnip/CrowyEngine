#pragma once

// The Island's numbers, read alike by C++ and Slang: literals only, each
// macro the components of one value.

// the island: an ellipsoid of these radii, its centre this far under the sea
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

// radians a pixel spans at the frame's centre: 60 deg over 720 rows
#define ISLAND_PIXEL_ANGLE 0.00160375f
