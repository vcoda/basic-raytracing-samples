#version 460
#extension GL_GOOGLE_include_directive: require
#include "common.h"

void main()
{
    vec3 normal;
    vec2 texCoord;
    interpolate(normal, texCoord);
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 l = normalize(lightPos - hitPos);
    vec3 n = normalize(mat3(normalMatrices[gl_InstanceID]) * normal);
    oColor = max(dot(n, l), 0).xxx;
}
