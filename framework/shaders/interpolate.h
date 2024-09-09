#ifndef interpolate_h
#define interpolate_h

float interpolate(float x, float y, float z, vec3 bar)
{
    return dot(vec3(x, y, z), bar);
}

vec2 interpolate(vec2 a, vec2 b, vec2 c, vec3 bar)
{
    return a * bar.x + b * bar.y + c * bar.z;
}

vec3 interpolate(vec3 a, vec3 b, vec3 c, vec3 bar)
{
    return a * bar.x + b * bar.y + c * bar.z;
}

vec4 interpolate(vec4 a, vec4 b, vec4 c, vec3 bar)
{
    return a * bar.x + b * bar.y + c * bar.z;
}

#endif // interpolate_h
