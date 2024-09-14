#include "interpolate.h"

struct Vertex
{
    vec3 position;
    uint normal;
    vec2 texCoord;
    uint color;
    uint padding;
};

layout(buffer_reference) buffer readonly VertexBuffer {
    Vertex vertices[];
};
layout(buffer_reference) buffer readonly IndexBuffer {
    uint indices[];
};

void interpolateTriangleAttributes(
    uint64_t vbAddr, uint64_t ibAddr, vec3 barycentrics,
    inout vec3 normal, inout vec2 texCoord, inout vec3 color)
{
    uint i = gl_PrimitiveID * 3;
    IndexBuffer ib = IndexBuffer(ibAddr);
    uvec3 face;
    face.x = ib.indices[i];
    face.y = ib.indices[i + 1];
    face.z = ib.indices[i + 2];
    VertexBuffer vb = VertexBuffer(vbAddr);
    Vertex v0 = vb.vertices[face.x];
    Vertex v1 = vb.vertices[face.y];
    Vertex v2 = vb.vertices[face.z];
    vec3 n0 = unpackSnorm4x8(v0.normal).xyz;
    vec3 n1 = unpackSnorm4x8(v1.normal).xyz;
    vec3 n2 = unpackSnorm4x8(v2.normal).xyz;
    normal = interpolate(n0, n1, n2, barycentrics);
    vec2 tc0 = v0.texCoord;
    vec2 tc1 = v1.texCoord;
    vec2 tc2 = v2.texCoord;
    texCoord = interpolate(tc0, tc1, tc2, barycentrics);
    vec3 c0 = unpackUnorm4x8(v0.color).rgb;
    vec3 c1 = unpackUnorm4x8(v1.color).rgb;
    vec3 c2 = unpackUnorm4x8(v2.color).rgb;
    color = interpolate(c0, c1, c2, barycentrics);
}
