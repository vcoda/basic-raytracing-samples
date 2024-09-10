#version 460
#extension GL_EXT_ray_tracing: require

layout(location = 0) rayPayloadInEXT vec3 oColor;

hitAttributeEXT vec2 hitAttrib;

void main()
{
    if (gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT)
        oColor = vec3(1 - hitAttrib.x - hitAttrib.y, hitAttrib.xy);
    else
        oColor = vec3(0.2, 0.2, 0.2);
}
