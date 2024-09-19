#version 460
#extension GL_GOOGLE_include_directive: require
#include "common.h"

void main()
{
    vec3 normal;
    vec2 texCoord;
    interpolate(normal, texCoord);
    oColor = texture(diffuseMap, texCoord).rgb;
}
