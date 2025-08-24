#pragma once
#include "magma/magma.h"

std::unique_ptr<magma::ImageView> loadImage(const std::string& fileName,
    magma::lent_ptr<magma::CommandBuffer> cmdBuffer, std::shared_ptr<magma::Allocator> allocator);
std::unique_ptr<magma::ImageView> loadBlankImage(magma::lent_ptr<magma::CommandBuffer> cmdBuffer,
    std::shared_ptr<magma::Allocator> allocator);
