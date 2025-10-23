#version 460
#extension GL_EXT_ray_tracing: require

struct Payload
{
    vec3 rdx;
    vec3 rdy;
    vec3 color;
};

layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    payload.color = vec3(0);
}
