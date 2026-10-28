#pragma once

// Render's enums for C++ and Slang, each as X(shader prefix, name, value): C++
// builds its enum classes from them, a shader reads <prefix><name>.

// Shadow is the lights' shadow term: 1 white, 0 black
#define CROWY_DEBUG_MODES(X)                                                   \
    X(DebugMode, Lit, 0)                                                       \
    X(DebugMode, Unshaded, 1)                                                  \
    X(DebugMode, Normals, 2)                                                   \
    X(DebugMode, Depth, 3)                                                     \
    X(DebugMode, Overdraw, 4)                                                  \
    X(DebugMode, Shadow, 5)

#define CROWY_LIGHT_KINDS(X)                                                   \
    X(LightKind, Directional, 0)                                               \
    X(LightKind, Point, 1)                                                     \
    X(LightKind, Spot, 2)

// no shadow-map slot: the shader skips its lookup
#define CROWY_NO_SHADOW 0xFFFFFFFFu

// Hard is one texel and one compare, exactly 0 or 1; Pcf2x2 weighs four
// compares bilinearly, a soft edge a texel wide
#define CROWY_SHADOW_FILTERS(X)                                                \
    X(ShadowFilter, Hard, 0)                                                   \
    X(ShadowFilter, Pcf2x2, 1)

// NoShadowReceive: lights reach the surface unshadowed, as on a painted
// backdrop
#define CROWY_MATERIAL_FLAGS(X)                                                \
    X(MaterialFlag, None, 0u)                                                  \
    X(MaterialFlag, NoShadowReceive, 1u << 0)

#ifdef __cplusplus
#define CROWY_ENUMERATOR(PREFIX, NAME, VALUE) NAME = VALUE,
#define CROWY_ENUM_ENTRY(PREFIX, NAME, VALUE) CROWY_ENUM_VALUE(NAME)
#else
#define CROWY_SHADER_CONSTANT(PREFIX, NAME, VALUE)                             \
    static const uint PREFIX##NAME = VALUE;
CROWY_DEBUG_MODES(CROWY_SHADER_CONSTANT)
CROWY_LIGHT_KINDS(CROWY_SHADER_CONSTANT)
CROWY_SHADOW_FILTERS(CROWY_SHADER_CONSTANT)
CROWY_MATERIAL_FLAGS(CROWY_SHADER_CONSTANT)
static const uint NoShadow = CROWY_NO_SHADOW;
#endif
