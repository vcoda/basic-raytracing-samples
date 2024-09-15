#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_shader_explicit_arithmetic_types_int64: require
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_GOOGLE_include_directive: require
#include "triangleAttribs.h"
#include "brdf.h"

struct Mesh
{
    uint64_t vbAddr;
    uint64_t ibAddr;
};

layout(shaderRecordEXT) buffer LightSource {
    vec3 lightPos;
};
layout(set = 0, binding = 0) uniform View {
    mat4x4 viewInv;
    mat4x4 projInv;
    mat4x4 viewProjInv;
};
layout(set = 0, binding = 2) buffer readonly References {
    Mesh meshes[];
};
layout(set = 0, binding = 3) uniform sampler2D diffuseMap;
layout(set = 0, binding = 4) uniform Transform {
    mat4 normalMatrix;
};

layout(location = 0) rayPayloadInEXT vec3 oColor;

hitAttributeEXT vec2 hit;

void main()
{
    // interpolate per-vertex attributes
    Mesh mesh = meshes[gl_GeometryIndexEXT];
    vec3 barycentrics = vec3(1 - hit.x - hit.y, hit.xy);
    vec3 normal, color;
    vec2 texCoord;
    interpolateTriangleAttributes(mesh.vbAddr, mesh.ibAddr, barycentrics,
        normal, texCoord, color);

    // compute world-space normal, view and light vectors
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec4 viewPos = viewInv * vec4(0, 0, 0, 1);
    vec3 v = normalize(viewPos.xyz - hitPos);
    vec3 l = normalize(lightPos - hitPos);
    vec3 n = normalize(mat3(normalMatrix) * normal);

    // compute Phong lighting
    vec3 albedo = texture(diffuseMap, texCoord).rgb;
    vec3 Ka = albedo * 0.1;
    vec3 Kd = albedo;
    vec3 Ks = Kd;
    float shininess = 16;
    oColor = phong(n, l, v, Ka, Kd, Ks, shininess);
}
