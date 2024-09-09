#pragma once
#include "magma/magma.h"

std::shared_ptr<magma::ImageView> loadImage(const std::string& fileName,
    std::shared_ptr<magma::CommandBuffer> cmdBuffer);