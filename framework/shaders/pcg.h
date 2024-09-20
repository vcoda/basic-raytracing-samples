/* Hash Functions for GPU Rendering.
   Mark Jarzynski, Marc Olano
   Journal of Computer Graphics Techniques (JCGT), vol. 9, no. 3, 21-38, 2020
   http://jcgt.org/published/0009/03/02/
   https://www.shadertoy.com/view/XlGcRh */

#ifndef pcg_h
#define pcg_h

#define PCG_MUL 1664525u
#define PCG_ADD 1013904223u

vec2 pcg2d(uvec2 v)
{
    v = v * PCG_MUL + PCG_ADD;
    v.x += v.y * PCG_MUL;
    v.y += v.x * PCG_MUL;
    v = v ^ (v >> 16u);
    v.x += v.y * PCG_MUL;
    v.y += v.x * PCG_MUL;
    v = v ^ (v >> 16u);
    return v / float(0xffffffffu);
}

vec3 pcg3d(uvec3 v)
{
    v = v * PCG_MUL + PCG_ADD;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v / float(0xffffffffu);
}

vec4 pcg4d(uvec4 v)
{
    v = v * PCG_MUL + PCG_ADD;
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    v ^= v >> 16u;
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    return v / float(0xFFFFFFFFu);
}

#endif // pcg_h
