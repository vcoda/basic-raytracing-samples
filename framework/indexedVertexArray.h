#pragma once
#include "utilities.h"

template<class Vertex, class Index>
class IndexedVertexArray
{
    static_assert(std::is_integral<Index>::value && std::is_unsigned<Index>::value,
        "index type should be unsigned integral");
    static_assert(sizeof(Index) > sizeof(uint8_t),
        "index type should be at least of short type");

public:
    explicit IndexedVertexArray(const std::vector<Vertex>& array)
    {
        std::unordered_map<Vertex, Index> hashMap;
        Index index = 0u;
        vertices.reserve(array.size());
        for (auto const& v: array)
        {
            if (0u == hashMap.count(v))
            {
                hashMap[v] = index++;
                vertices.push_back(v);
            }
        }
        indices.reserve(array.size());
        for (auto const& v: array)
        {
            index = hashMap[v];
            indices.push_back(index);
        }
        vertices.shrink_to_fit();
        indices.shrink_to_fit();
    }

    void changeWindingOrder() noexcept
    {
        for (uint32_t i = 0, n = (uint32_t)indices.size(); i < n; i += 3)
        {
            std::swap(indices[i + 1], indices[i + 2]);
        }
    }

    const vector<Vertex>& getVertices() const noexcept { return vertices; }
    const vector<Index>& getIndices() const noexcept { return indices; }

private:
    vector<Vertex> vertices;
    vector<Index> indices;
};
