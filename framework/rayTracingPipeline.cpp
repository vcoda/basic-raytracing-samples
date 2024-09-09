#include <fstream>
#include "rayTracingPipeline.h"

RayTracingPipeline::RayTracingPipeline(std::shared_ptr<magma::Device> device,
    const std::initializer_list<const char *> fileNames,
    const std::vector<magma::RayTracingShaderGroup>& shaderGroups,
    uint32_t maxPipelineRayRecursionDepth,
    std::shared_ptr<magma::PipelineLayout> layout,
    std::shared_ptr<magma::IAllocator> allocator /* null */):
    magma::RayTracingPipeline(device,
        loadShaders(device, fileNames),
        shaderGroups,
        maxPipelineRayRecursionDepth,
        std::move(layout),
        std::move(allocator))
{}

std::vector<magma::PipelineShaderStage> RayTracingPipeline::loadShaders(std::shared_ptr<magma::Device> device,
    const std::initializer_list<const char *> fileNames)
{
    std::vector<magma::PipelineShaderStage> shaderStages;
    for (auto const& fileName: fileNames)
        shaderStages.push_back(loadShader(device, fileName));
    return shaderStages;
}

magma::PipelineShaderStage RayTracingPipeline::loadShader(std::shared_ptr<magma::Device> device,
    const char *fileName)
{
    const std::string ext = ".spv";
    std::ifstream file(fileName + ext, std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("file \"" + std::string(fileName) + ext + "\" not found");
    std::vector<char> bytecode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytecode.size() % sizeof(magma::SpirvWord))
        throw std::runtime_error("size of \"" + std::string(fileName) + "\" bytecode must be a multiple of SPIR-V word");
    auto allocator = device->getHostAllocator();
    constexpr bool reflect = true;
    std::shared_ptr<magma::ShaderModule> module(std::make_shared<magma::ShaderModule>(std::move(device),
        reinterpret_cast<const magma::SpirvWord *>(bytecode.data()), bytecode.size(), 0,
        std::move(allocator), reflect, 0));
    const VkShaderStageFlagBits stage = module->getReflection()->getShaderStage();
    const char *const entrypoint = module->getReflection()->getEntryPointName(0);
    return magma::PipelineShaderStage(stage, std::move(module), entrypoint);
}
