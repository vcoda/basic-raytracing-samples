#version 460
#extension GL_EXT_ray_tracing: require

layout(shaderRecordEXT) buffer Color {
    vec3 missColor;
};

layout(location = 0) rayPayloadInEXT vec3 oColor;

void main()
{
    oColor = missColor;
}
