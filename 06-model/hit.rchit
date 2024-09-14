#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_shader_explicit_arithmetic_types_int64: require
#extension GL_GOOGLE_include_directive: require
#include "triangleAttribs.h"

struct Mesh
{
    uint64_t vbAddr;
    uint64_t ibAddr;
};

layout(shaderRecordEXT) buffer LightSource {
    vec3 lightPos;
};
layout(set = 0, binding = 2) buffer readonly References {
    Mesh meshes[];
};
layout(set = 0, binding = 3) uniform Transform {
    mat4 normalMatrix;
};

layout(location = 0) rayPayloadInEXT vec3 oColor;

void main()
{
    // load face normal and color
    Mesh mesh = meshes[gl_GeometryIndexEXT];
    vec3 normal, color;
    loadTriangleAttributes(mesh.vbAddr, mesh.ibAddr, normal, color);

    // compute world-space normal and light vectors
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 l = normalize(lightPos - hitPos);
    vec3 n = normalize(mat3(normalMatrix) * normal);

    // compute diffuse lighting
    vec3 ambient = color * 0.1;
    vec3 diffuse = color * max(dot(n, l), 0);
    oColor = ambient + diffuse;
}
