#version 460
#extension GL_EXT_ray_tracing: require

layout(location = 0) rayPayloadInEXT vec3 oColor;

void main()
{
    const vec3 lightPos = vec3(-10, -10, 10);
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 n = normalize(hitPoint);
    vec3 l = normalize(lightPos - hitPoint);
    oColor = max(dot(n, l), 0).xxx;
}