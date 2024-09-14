#pragma once
#include <memory>
#include <vector>
#include <cassert>

#include "alignedAllocator.h"

template<class T, class Alloc = std::allocator<T>>
class vector : public std::vector<T, Alloc>
{
public:
    std::size_t size_bytes() const noexcept { return std::vector<T, Alloc>::size() * sizeof(T); }
};

template<class Type>
using aligned_vector = std::vector<Type, utilities::aligned_allocator<Type>>;

namespace utilities
{
    aligned_vector<char> loadBinaryFile(const std::string& filename);
    void saveBinaryFile(const std::string& fileName, const void *data, size_t size);
    VkBool32 VKAPI_PTR reportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
        uint64_t object, size_t location, int32_t messageCode,
        const char *pLayerPrefix, const char *pMessage, void *pUserData);
} // namespace utilities
