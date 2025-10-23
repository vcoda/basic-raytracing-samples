#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_GOOGLE_include_directive: require
#include "interpolate.h"
#include "derivatives.h"

//#define USE_EXPLICIT_GRADIENTS
//#define DEBUG_MIPMAP

#ifdef USE_EXPLICIT_GRADIENTS
    #define textureFilter trilinearGrad
#else
    #define textureFilter trilinearLod
#endif

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

vec4 trilinearGrad(sampler2D image, vec2 uv, vec3 normal, vec3 rdx, vec3 rdy, float scale)
{
    vec3 dpdx, dpdy; 
    dPdxy(rdx, rdy, normal, dpdx, dpdy);
    vec2 duvdx = magProj(dpdx, normal);
    vec2 duvdy = magProj(dpdy, normal);
    return textureGrad(image, uv, duvdx * scale, duvdy * scale);
}

vec4 trilinearLod(sampler2D image, vec2 uv, vec3 normal, vec3 rdx, vec3 rdy, float scale)
{
    vec3 dpdx, dpdy; 
    dPdxy(rdx, rdy, normal, dpdx, dpdy);
    vec2 duvdx = magProj(dpdx, normal);
    vec2 duvdy = magProj(dpdy, normal);
    float rhox = length(duvdx);
    float rhoy = length(duvdy);
    float rho = max(rhox, rhoy) * scale;
    ivec2 size = textureSize(image, 0);
    float lod = log2(rho * max(size.x, size.y));
    float lod0 = floor(lod);
    float lod1 = lod0 + 1;
    vec4 col0 = textureLod(image, uv, lod0);
    vec4 col1 = textureLod(image, uv, lod1);
    return mix(col0, col1, fract(lod));
}

void loadTriangleAttributes(out vec3 normal, out vec2 uv)
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
}

void main()
{
    vec3 normal;
    vec2 uv;
    loadTriangleAttributes(normal, uv);
    if (useFiltering)
        payload.color = textureFilter(TEXTURE, uv, normalize(normal), payload.rdx, payload.rdy, 0.25).SWIZZLE;
    else
        payload.color = texture(TEXTURE, uv).SWIZZLE;
}
