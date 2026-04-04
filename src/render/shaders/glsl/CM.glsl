#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif
#include "cm_helpers.glsl"

uniform vec2  srcTFRange;
uniform vec2  dstTFRange;

uniform float srcRefLuminance;
uniform mat3  convertMatrix;

#if USE_ICC
uniform highp sampler3D iccLut3D;
uniform float           iccLutSize;
#endif

#if USE_SDR_MOD
uniform float sdrSaturation;
uniform float sdrBrightnessMultiplier;
#endif

#if USE_TONEMAP
uniform float maxLuminance;
uniform float dstMaxLuminance;
uniform float dstRefLuminance;
#endif
