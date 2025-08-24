#include "image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb/stb_image.h"

std::unique_ptr<magma::ImageView> loadImage(const std::string& fileName, magma::lent_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator)
{
    int width = 0, height = 0, channels = 0;
    unsigned char *data = stbi_load(fileName.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (data)
    {
        magma::Image::MipData mip;
        mip.extent.width = width;
        mip.extent.height = height;
        mip.extent.depth = 1;
        mip.texels = data;
        mip.size = width * height * sizeof(uint32_t);
        std::unique_ptr<magma::Image2D> image = std::make_unique<magma::Image2D>(std::move(cmdBuffer), VK_FORMAT_R8G8B8A8_UNORM,
            std::vector<magma::Image::MipData>{mip}, allocator);
        stbi_image_free(data);
        return std::make_unique<magma::UniqueImageView>(std::move(image), allocator->getHostAllocator());
    }
    return nullptr;
}

std::unique_ptr<magma::ImageView> loadBlankImage(magma::lent_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator)
{
    const uint8_t blank[4] = {0, 0, 0, 0};
    magma::Image::MipData mip;
    mip.extent.width = 1;
    mip.extent.height = 1;
    mip.extent.depth = 1;
    mip.texels = blank;
    mip.size = sizeof(uint32_t);
    std::unique_ptr<magma::Image2D> image = std::make_unique<magma::Image2D>(std::move(cmdBuffer), VK_FORMAT_R8G8B8A8_UNORM,
        std::vector<magma::Image::MipData>{mip}, allocator);
    return std::make_unique<magma::UniqueImageView>(std::move(image), allocator->getHostAllocator());
}
