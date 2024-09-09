#include "image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb/stb_image.h"

std::shared_ptr<magma::ImageView> loadImage(const std::string& fileName, std::shared_ptr<magma::CommandBuffer> cmdBuffer)
{
    int width = 0, height = 0, channels = 0;
    unsigned char *data = stbi_load(fileName.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    std::shared_ptr<magma::Image2D> image;
    if (data)
    {
        magma::Image::MipData mip;
        mip.extent.width = width;
        mip.extent.height = height;
        mip.extent.depth = 1;
        mip.texels = data;
        mip.size = width * height * sizeof(uint32_t);
        image = std::make_shared<magma::Image2D>(cmdBuffer, VK_FORMAT_R8G8B8A8_UNORM,
            std::vector<magma::Image::MipData>{mip}, nullptr, magma::Image::Initializer{},
            magma::Sharing(), memcpy);
        stbi_image_free(data);
    }
    return std::make_shared<magma::ImageView>(std::move(image));
}
