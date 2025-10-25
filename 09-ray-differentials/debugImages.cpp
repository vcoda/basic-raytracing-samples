#include "magma/magma.h"

std::unique_ptr<magma::UniqueImageView> createDebugCheckerboard(magma::lent_ptr<magma::CommandBuffer> cmdBuffer, uint32_t width, uint32_t height,
    std::shared_ptr<magma::Allocator> allocator /* nullptr */)
{
    auto texels = std::make_unique<magma::SrcTransferBuffer>(cmdBuffer->getDevice(), width * height, allocator);
    magma::map<uint8_t>(texels, [width, height](uint8_t *data)
    {   // Fill with checkerboard pattern
        const uint32_t cellWidth = width >> 5;
        const uint32_t cellHeight = height >> 6;
        for (uint32_t y = 0; y < height; ++y)
        {
            const uint32_t cy = y / cellHeight;
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t cx = x / cellWidth;
                data[y * width + x] = (cx + cy) & 1 ? 0x0 : 0xFF;
            }
        }
    });
    magma::Image::Initializer initializer;
    initializer.srcTransfer = VK_TRUE;
    const uint32_t mipLevels = (uint32_t)floor(log2(std::max(width, height))) + 1;
    auto image = std::make_unique<magma::Image2D>(cmdBuffer->getDevice(),
        VK_FORMAT_R8_UNORM, VkExtent3D{width, height, 1}, mipLevels, std::move(allocator), initializer);
    MAGMA_ASSERT(cmdBuffer->allowsReset());
    MAGMA_ASSERT(cmdBuffer->getState() != magma::CommandBuffer::State::Recording);
    cmdBuffer->reset();
    if (cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
    {
        image->copyMipWithTransition(cmdBuffer.get(), 0, 0, texels, {0, 0, 0}, {0, 0, 0},
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        magma::aux::generateMipmap(image, 0, VK_FILTER_LINEAR, cmdBuffer.get());
        cmdBuffer->end();
        magma::finish(std::move(cmdBuffer));
    }
    return std::make_unique<magma::UniqueImageView>(std::move(image));
}

std::unique_ptr<magma::UniqueImageView> createDebugMipmap(magma::lent_ptr<magma::CommandBuffer> cmdBuffer,
    std::shared_ptr<magma::Allocator> allocator /* nullptr */)
{
    constexpr uint32_t size = 512;
    const uint32_t mipLevels = (uint32_t)floor(log2(size)) + 1;
    auto image = std::make_unique<magma::Image2D>(cmdBuffer->getDevice(),
        VK_FORMAT_R8G8B8A8_UNORM, VkExtent3D{size, size, 1}, mipLevels, allocator);
    auto texels = std::make_unique<magma::SrcTransferBuffer>(cmdBuffer->getDevice(), image->getTexelCount() * sizeof(uint32_t), std::move(allocator));
    magma::map<uint8_t>(texels, [&image](uint8_t *data)
    {
        const uint32_t mipColors[] = {0xFF0000FF, 0xFF00FFFF, 0xFF00FF00, 0xFFFFFF00, 0xFFFF0000, 0xFFFF00FF, 0xFFFFFFFF, 0xFFFFFFFF};
        VkDeviceSize bufferOffset = 0;
        for (uint32_t level = 0; level < image->getMipLevels(); ++level)
        {   // Fill each mip level with distinct color
            const uint32_t texelCount = image->getLevelTexelCount(level);
            const uint32_t color = level < 7 ? mipColors[level] : 0x0;
            uint32_t *begin = (uint32_t *)(data + bufferOffset);
            std::fill(begin, begin + texelCount, color);
            bufferOffset += texelCount * sizeof(uint32_t);
        }
    });
    MAGMA_ASSERT(cmdBuffer->allowsReset());
    MAGMA_ASSERT(cmdBuffer->getState() != magma::CommandBuffer::State::Recording);
    cmdBuffer->reset();
    if (cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
    {   // Layout transition to destination of a transfer command
        const VkImageSubresourceRange subresourceRange = image->getSubresourceRange(0);
        cmdBuffer->pipelineBarrier(VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            magma::ImageMemoryBarrier(image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange));
        VkDeviceSize bufferOffset = 0;
        for (uint32_t level = 0; level < image->getMipLevels(); ++level)
        {
            const magma::Image::CopyLayout copyLayout{bufferOffset, 0, 0};
            image->copyMip(cmdBuffer.get(), level, 0, texels, copyLayout);
            bufferOffset += image->getLevelTexelCount(level) * sizeof(uint32_t);
        }
        // Layout transition to fragment shader read only
        cmdBuffer->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            magma::ImageMemoryBarrier(image.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange));
        cmdBuffer->end();
        magma::finish(std::move(cmdBuffer));
    }
    return std::make_unique<magma::UniqueImageView>(std::move(image));
}
