#include "image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb/stb_image.h"

std::shared_ptr<magma::ImageView> loadImage(const std::string& fileName, std::shared_ptr<magma::CommandBuffer> cmdBuffer)
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
        std::shared_ptr<magma::Image2D> image = std::make_shared<magma::Image2D>(cmdBuffer, VK_FORMAT_R8G8B8A8_UNORM,
            std::vector<magma::Image::MipData>{mip}, nullptr, magma::Image::Initializer{}, magma::Sharing(), memcpy);
        stbi_image_free(data);
        return std::make_shared<magma::ImageView>(std::move(image));
    }
    return nullptr;
}

std::shared_ptr<magma::ImageView> loadBlankImage(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
{
    const uint8_t blank[4] = {0, 0, 0, 0};
    magma::Image::MipData mip;
    mip.extent.width = 1;
    mip.extent.height = 1;
    mip.extent.depth = 1;
    mip.texels = blank;
    mip.size = sizeof(uint32_t);
    std::shared_ptr<magma::Image2D> image = std::make_shared<magma::Image2D>(cmdBuffer, VK_FORMAT_R8G8B8A8_UNORM,
        std::vector<magma::Image::MipData>{mip}, nullptr, magma::Image::Initializer{}, magma::Sharing(), memcpy);
    return std::make_shared<magma::ImageView>(std::move(image));
}
