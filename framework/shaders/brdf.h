#ifndef brdf_h
#define brdf_h

vec3 phong(vec3 n, vec3 l, vec3 v, vec3 Ka, vec3 Kd, vec3 Ks, float shininess)
{
    vec3 r = reflect(-l, n);
    float NdL = dot(n, l);
    float RdV = dot(r, v);
    return Ka + Kd * max(0, NdL) + Ks * pow(max(0, RdV), shininess);
}

vec3 blinnPhong(vec3 n, vec3 l, vec3 v, vec3 Ka, vec3 Kd, vec3 Ks, float shininess)
{
    vec3 h = normalize(l + v);
    float NdL = dot(n, l);
    float NdH = dot(n, h);
    return Ka + Kd * max(0, NdL) + Ks * pow(max(0, NdH), shininess);
}

#endif // brdf_h
