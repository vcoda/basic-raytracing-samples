#version 460
#extension GL_EXT_ray_tracing: require

layout(shaderRecordEXT) buffer LightSource {
    vec3 lightPos;
};
layout(set = 0, binding = 2) buffer Vertices {
    vec4 vertices[];
};
layout(set = 0, binding = 3) uniform Transform {
    mat4 normalMatrix;
};

layout(location = 0) rayPayloadInEXT vec3 oColor;

hitAttributeEXT vec2 hitAttrib;

vec3 faceNormal(uint primitiveID)
{
    uint offset = primitiveID * 3;
    vec3 v0 = vertices[offset].xyz;
    vec3 v1 = vertices[offset + 1].xyz;
    vec3 v2 = vertices[offset + 2].xyz;
    return cross(v1 - v0, v2 - v0);
}

void main()
{
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 l = normalize(lightPos - hitPoint);
    vec3 normal = faceNormal(gl_PrimitiveID);
    vec3 n = normalize(mat3(normalMatrix) * normal);
    oColor = max(dot(n, l), 0).xxx;
}
