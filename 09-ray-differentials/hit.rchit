#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_GOOGLE_include_directive: require
#include "interpolate.h"
#include "derivatives.h"

//#define DEBUG_MIPMAP
//#define PSEUDO_INVERSE_JACOBIAN

#ifdef DEBUG_MIPMAP
    #define TEXTURE mipmap
    #define SWIZZLE xyz
#else
    #define TEXTURE checkerboard
    #define SWIZZLE rrr
#endif

struct Vertex
{
    vec3 pos;
    float u;
    vec3 normal;
    float v;
};

struct Payload
{
    vec3 rdx;
    vec3 rdy;
    vec3 color;
};

layout(set = 0, binding = 1) uniform Parameters {
    bool useFiltering;
};
layout(set = 0, binding = 3) uniform sampler2D checkerboard;
layout(set = 0, binding = 4) uniform sampler2D mipmap;
layout(set = 0, binding = 5) readonly buffer Vertices {
    Vertex vertices[];
};
layout(set = 0, binding = 6) readonly buffer Indices {
    uint indices[];
};
layout(location = 0) rayPayloadInEXT Payload payload;

hitAttributeEXT vec2 hit;

vec4 textureGrad2D(sampler2D image, vec2 uv, vec3 normal, vec3 rdx, vec3 rdy,
    vec3 dp1, vec3 dp2, vec2 duv1, vec2 duv2)
{
    vec3 dpdx, dpdy;
    dPdxy(rdx, rdy, normal, dpdx, dpdy);
#ifdef PSEUDO_INVERSE_JACOBIAN
    mat3x2 invJ = inverseJacobian(dp1, dp2, duv1, duv2);
#else
    mat3x3 J = jacobian(dp1, dp2, duv1, duv2);
    mat3x3 invJ = inverse(J);
#endif
    vec2 duvdx = (invJ * dpdx).xy;
    vec2 duvdy = (invJ * dpdy).xy;
    return textureGrad(image, uv, duvdx, duvdy);
}

void loadTriangleAttributes(out vec3 normal, out vec2 uv,
    out vec3 dp1, out vec3 dp2, out vec2 duv1, out vec2 duv2)
{
    uint i = gl_PrimitiveID * 3;
    uvec3 tri;
    tri.x = indices[i];
    tri.y = indices[i + 1];
    tri.z = indices[i + 2];
    Vertex v0 = vertices[tri.x];
    Vertex v1 = vertices[tri.y];
    Vertex v2 = vertices[tri.z];
    vec3 n0 = v0.normal;
    vec3 n1 = v1.normal;
    vec3 n2 = v2.normal;
    vec2 uv0 = vec2(v0.u, v0.v);
    vec2 uv1 = vec2(v1.u, v1.v);
    vec2 uv2 = vec2(v2.u, v2.v);
    normal = interpolate(n0, n1, n2, hit.x, hit.y);
    uv = interpolate(uv0, uv1, uv2, hit.x, hit.y);
    dp1 = v1.pos - v0.pos;
    dp2 = v2.pos - v0.pos;
    duv1 = uv1 - uv0;
    duv2 = uv2 - uv0;
}

void main()
{
    vec3 normal, dp1, dp2;
    vec2 uv, duv1, duv2;
    loadTriangleAttributes(normal, uv, dp1, dp2, duv1, duv2);
    if (useFiltering)
        payload.color = textureGrad2D(TEXTURE, uv, normalize(normal), payload.rdx, payload.rdy, dp1, dp2, duv1, duv2).SWIZZLE;
    else
        payload.color = texture(TEXTURE, uv).SWIZZLE;
}
