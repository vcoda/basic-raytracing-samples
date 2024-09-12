#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_GOOGLE_include_directive: require
#include "interpolate.h"

layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevel;
layout(set = 0, binding = 2) buffer TexCoords {
    vec2 texCoords[];
};
layout(set = 0, binding = 3) uniform sampler2D diffuse;

layout(location = 0) rayPayloadInEXT vec3 oColor;

hitAttributeEXT vec2 hit;

void main()
{
    uint offset = gl_PrimitiveID * 3;
    vec2 uv0 = texCoords[offset];
    vec2 uv1 = texCoords[offset + 1];
    vec2 uv2 = texCoords[offset + 2];
    vec2 uv = interpolate(uv0, uv1, uv2, hit.x, hit.y);
    vec4 albedo = texture(diffuse, uv);
    if (albedo.a > 0)
        oColor = albedo.rgb;
    else
    {   // continue trace if we hit transparent texel
        vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3 dir = gl_WorldRayDirectionEXT;
        float tmin = 0.01, tmax = 10;
        traceRayEXT(topLevel,
            gl_RayFlagsOpaqueEXT,
            0xFF, // cullMask
            0, // sbtRecordOffset
            0, // sbtRecordStride
            0, // missIndex
            origin, tmin,
            dir, tmax,
            0); // payload
    }
}
