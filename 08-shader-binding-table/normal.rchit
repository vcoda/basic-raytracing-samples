#version 460
#extension GL_GOOGLE_include_directive: require
#include "common.h"

void main()
{
    vec2 texCoord;
    interpolate(oColor, texCoord);
}
