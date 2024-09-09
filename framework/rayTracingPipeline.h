#pragma once
#include "magma/magma.h"

class RayTracingPipeline : public magma::RayTracingPipeline
{
public:
    explicit RayTracingPipeline(std::shared_ptr<magma::Device> device,
        const std::initializer_list<const char *> fileNames,
        const std::vector<magma::RayTracingShaderGroup>& shaderGroups,
        uint32_t maxPipelineRayRecursionDepth,
        std::shared_ptr<magma::PipelineLayout> layout,
        std::shared_ptr<magma::IAllocator> allocator = nullptr);

private:
    static std::vector<magma::PipelineShaderStage> loadShaders(
        std::shared_ptr<magma::Device> device,
        const std::initializer_list<const char *> fileNames);
    static magma::PipelineShaderStage loadShader(
        std::shared_ptr<magma::Device> device, const char *fileName);
};
