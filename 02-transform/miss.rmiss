#version 460
#extension GL_EXT_ray_tracing: require

layout(location = 0) rayPayloadInEXT vec3 oColor;

void main()
{
    oColor = vec3(.5);
}
