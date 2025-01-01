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

    template<class T>
    inline std::unique_ptr<magma::AccelerationStructureInputBuffer> makeInputBuffer(const T& element,
        std::shared_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator = nullptr)
    {
        return std::make_unique<magma::AccelerationStructureInputBuffer>(std::move(cmdBuffer),
            sizeof(T), &element, std::move(allocator));
    }

    template<class T, std::size_t N>
    inline std::unique_ptr<magma::AccelerationStructureInputBuffer> makeInputBuffer(const T (&array)[N],
        std::shared_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator = nullptr)
    {
        static_assert(N > 0, "invalid array size");
        return std::make_unique<magma::AccelerationStructureInputBuffer>(std::move(cmdBuffer),
            sizeof(T) * N, array, std::move(allocator));
    }

    template<class T, std::size_t N>
    inline std::unique_ptr<magma::StorageBuffer> makeStorageBuffer(const T (&array)[N],
        std::shared_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator = nullptr)
    {
        return std::make_unique<magma::StorageBuffer>(std::move(cmdBuffer), sizeof(T) * N, array, std::move(allocator));
    }

    template<class T>
    inline std::unique_ptr<magma::StorageBuffer> makeStorageBuffer(const vector<T> array,
        std::shared_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator = nullptr)
    {
        return std::make_unique<magma::StorageBuffer>(std::move(cmdBuffer), array.size_bytes(), array.data(), std::move(allocator));
    }
} // namespace utilities
