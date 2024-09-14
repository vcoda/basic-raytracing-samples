#pragma once

struct alignas(16) Vertex
{
    rapid::float3 pos;
    int8_t normal[4];
    rapid::float2 texCoord;
    uint8_t color[4];
    uint32_t padding;
};

inline bool operator==(const Vertex& a, const Vertex& b) noexcept
{
    return !memcmp(&a, &b, sizeof(a));
}

namespace std
{
    template<> struct hash<Vertex>
    {
        inline size_t operator()(const Vertex& v) const
        {
            return magma::core::hashArgs(
                v.pos.x,
                v.pos.y,
                v.pos.z,
                v.normal[0],
                v.normal[1],
                v.normal[2],
                v.texCoord.x,
                v.texCoord.y,
                v.color[0],
                v.color[1],
                v.color[2]);
        }
    };
} // std
