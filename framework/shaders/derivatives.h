#ifndef derivatives_h
#define derivatives_h

// https://iquilezles.org/articles/filteringrm/
void dPdxy(vec3 rdx, vec3 rdy, vec3 n, out vec3 dpdx, out vec3 dpdy)
{
    vec3 rd = gl_WorldRayDirectionEXT;
    float NdD = dot(n, rd);
    dpdx = gl_HitTEXT * (rdx * NdD / dot(rdx, n) - rd);
    dpdy = gl_HitTEXT * (rdy * NdD / dot(rdy, n) - rd);
}

void dPdxyOrtho(vec3 rox, vec3 roy, vec3 n, out vec3 dpdx, out vec3 dpdy)
{
    vec3 rdx = gl_WorldRayOriginEXT - rox;
    vec3 rdy = gl_WorldRayOriginEXT - roy;
    vec3 rd = gl_WorldRayDirectionEXT;
    rd /= dot(rd, n);
    dpdx = rdx - rd * dot(rdx, n);
    dpdy = rdy - rd * dot(rdy, n);
}

mat3x3 jacobian(vec3 dp1, vec3 dp2, vec2 duv1, vec2 duv2)
{
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    float invDet = 1/det;
    vec3 dpdu = (dp1 * duv2.y - dp2 * duv1.y) * invDet;
    vec3 dpdv = (-dp1 * duv2.x + dp2 * duv1.x) * invDet;
    return mat3x3(dpdu, dpdv, cross(dpdu, dpdv));
}

// J^+ = (J^T * J)^-1 * J^T
mat3x2 pseudoInverseJacobian(vec3 dp1, vec3 dp2, vec2 duv1, vec2 duv2)
{
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    float invDet = 1/det;
    vec3 dpdu = (dp1 * duv2.y - dp2 * duv1.y) * invDet;
    vec3 dpdv = (-dp1 * duv2.x + dp2 * duv1.x) * invDet;
    mat2 JTJ = mat2(dot(dpdu, dpdu), dot(dpdu, dpdv),
                    dot(dpdu, dpdv), dot(dpdv, dpdv));
    mat2 invJTJ = inverse(JTJ);
    mat3x2 JT = transpose(mat2x3(dpdu, dpdv));
    return invJTJ * JT;
}

#endif // derivatives_h
