#version 460
#extension GL_GOOGLE_include_directive: require
#include "common.h"
#include "brdf.h"

void main()
{
    vec3 normal;
    vec2 texCoord;
    interpolate(normal, texCoord);

    // compute world-space normal, view and light vectors
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec4 viewPos = viewInv * vec4(0, 0, 0, 1);
    vec3 v = normalize(viewPos.xyz - hitPos);
    vec3 l = normalize(lightPos - hitPos);
    vec3 n = normalize(mat3(normalMatrices[gl_InstanceID]) * normal);

    // compute Phong lighting
    vec3 albedo = texture(diffuseMap, texCoord).rgb;
    vec3 Ka = albedo * 0.1;
    vec3 Kd = albedo;
    vec3 Ks = Kd;
    float shininess = 4;
    oColor = phong(n, l, v, Ka, Kd, Ks, shininess);
}
