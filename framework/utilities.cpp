#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX 1
#endif

#include <vulkan/vulkan.h>

#include "utilities.h"
#include "magma/magma.h"

namespace utilities
{
aligned_vector<char> loadBinaryFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        const std::string msg = "failed to open file \"" + filename + "\"";
        throw std::runtime_error(msg.c_str());
    }
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    aligned_vector<char> binary(static_cast<size_t>(size));
    file.read(binary.data(), size);
    file.close();
    return binary;
}

void saveBinaryFile(const std::string& fileName, const void *data, size_t size)
{
    std::ofstream file(fileName, std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        const std::string msg = "failed to create file \"" + fileName + "\"";
        throw std::runtime_error(msg.c_str());
    }
    file.write((char *)data, size);
}

VkBool32 VKAPI_PTR reportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
    uint64_t object, size_t location, int32_t messageCode,
    const char *pLayerPrefix, const char *pMessage, void *pUserData)
{
    if (strstr(pMessage, "Extension"))
        return VK_FALSE;
    std::stringstream msg;
    msg << "[" << pLayerPrefix << "] " << pMessage << "\n";
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        std::cerr << msg.str();
    else
        std::cout << msg.str();
    return VK_FALSE;
}
} // namespace utilities
