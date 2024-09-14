#ifndef interpolate_h
#define interpolate_h

float interpolate(float a, float b, float c, float u, float v)
{
    return dot(vec3(a, b, c), vec3(1 - u - v, u, v));
}

vec2 interpolate(vec2 a, vec2 b, vec2 c, float u, float v)
{
    return a * (1 - u - v) + b * u + c * v;
}

vec3 interpolate(vec3 a, vec3 b, vec3 c, float u, float v)
{
    return a * (1 - u - v) + b * u + c * v;
}

vec4 interpolate(vec4 a, vec4 b, vec4 c, float u, float v)
{
    return a * (1 - u - v) + b * u + c * v;
}

vec2 interpolate(vec2 a, vec2 b, vec2 c, vec3 bar)
{
    return a * bar.x + b * bar.y + c * bar.z;
}

vec3 interpolate(vec3 a, vec3 b, vec3 c, vec3 bar)
{
    return a * bar.x + b * bar.y + c * bar.z;
}

#endif // interpolate_h
