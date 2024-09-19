#extension GL_EXT_ray_tracing: require
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_shader_explicit_arithmetic_types_int64: require
#include "triangleAttribs.h"

struct Mesh
{
    uint64_t vbAddr;
    uint64_t ibAddr;
};

layout(set = 0, binding = 0) uniform View {
    mat4x4 viewInv;
    mat4x4 projInv;
    mat4x4 viewProjInv;
};
layout(set = 0, binding = 2) buffer readonly References {
    Mesh meshes[];
};
layout(set = 0, binding = 3) buffer readonly Transforms {
    mat4 normalMatrices[];
};
layout(set = 0, binding = 4) uniform LightSource {
    vec3 lightPos;
};
layout(set = 0, binding = 5) uniform sampler2D diffuseMap;

layout(location = 0) rayPayloadInEXT vec3 oColor;

hitAttributeEXT vec2 hit;

void interpolate(out vec3 normal, out vec2 texCoord)
{
    Mesh mesh = meshes[gl_GeometryIndexEXT];
    vec3 barycentrics = vec3(1 - hit.x - hit.y, hit.xy);
    vec3 color;
    interpolateTriangleAttributes(mesh.vbAddr, mesh.ibAddr, barycentrics,
        normal, texCoord, color);
}
