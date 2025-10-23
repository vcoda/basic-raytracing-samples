#ifndef derivatives_h
#define derivatives_h

// https://iquilezles.org/articles/filteringrm/
void dPdxy(vec3 rdx, vec3 rdy, vec3 n,
    out vec3 dpdx, out vec3 dpdy)
{
    float NdD = dot(n, gl_WorldRayDirectionEXT.xyz);
    dpdx = gl_RayTmaxEXT * (rdx * NdD / dot(rdx, n) - gl_WorldRayDirectionEXT.xyz);
    dpdy = gl_RayTmaxEXT * (rdy * NdD / dot(rdy, n) - gl_WorldRayDirectionEXT.xyz);
}

#define PLANE_XY 0
#define PLANE_XZ 1
#define PLANE_YZ 2

int magPlane(vec3 n)
{
    n = abs(n);
    bvec3 b = greaterThanEqual(n.xxy, n.yzz);
    return all(b.xy) ? PLANE_YZ : (b.z ? PLANE_XZ : PLANE_XY);
}

vec2 magProj(vec3 v, vec3 n)
{
    n = abs(n);
    bvec3 b = greaterThanEqual(n.xxy, n.yzz);
    return all(b.xy) ? v.yz : (b.z ? v.xz : v.xy);
}

#endif // derivatives_h
