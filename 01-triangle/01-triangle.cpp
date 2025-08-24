#include "../framework/vulkanRtApp.h"

class TriangleApp : public VulkanRayTracingApp
{
    struct TopLevelAccelerationStructureTable
    {
        magma::descriptor::AccelerationStructure topLevel = 0;
    } setTable;

    magma::AccelerationStructureTriangles tris;
    magma::AccelerationStructureInstances instance;
    std::unique_ptr<magma::AccelerationStructureInputBuffer> vertexBuffer;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    std::unique_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
    std::unique_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::unique_ptr<magma::DescriptorSet> descriptorSet;
    std::unique_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

public:
    TriangleApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Hello, triangle!"), 512, 512)
    {
        createGeometry();
        createAccelerationStructures();
        buildAccelerationStructures();
        setupDescriptorSet();
        setupPipeline();
        for (size_t i = 0; i < commandBuffers.size(); ++i)
            recordCommandBuffer(i);
    }

    void render(uint32_t bufferIndex) override
    {
        submitCommandBuffer(bufferIndex);
    }

    void createGeometry()
    {
        const magma::vt::Pos3f vertices[] = {
            { 0.0f,-0.3f, 0.f},
            {-0.6f, 0.3f, 0.f},
            { 0.6f, 0.3f, 0.f}
        };
        vertexBuffer = std::make_unique<magma::AccelerationStructureInputBuffer>(cmdBufferCopy, sizeof(vertices), vertices, allocator);
        tris = magma::AccelerationStructureTriangles(VK_FORMAT_R32G32B32_SFLOAT, vertexBuffer.get());
    }

    void createAccelerationStructures()
    {
        bottomLevel = std::make_unique<magma::BottomLevelAccelerationStructure>(device,
            std::list<magma::AccelerationStructureGeometry>{tris},
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            allocator);
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(device,
            1, allocator);
        instance = magma::AccelerationStructureInstances(instanceBuffer.get());
        instanceBuffer->getInstance(0).accelerationStructureReference = bottomLevel->getReference();
        topLevel = std::make_unique<magma::TopLevelAccelerationStructure>(device, instance,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            allocator);
    }

    void buildAccelerationStructures()
    {
        const VkDeviceSize maxSize = std::max(bottomLevel->getBuildScratchSize(), topLevel->getBuildScratchSize());
        std::unique_ptr<magma::Buffer> scratchBuffer = allocateScratchBuffer(maxSize);
        cmdCompute->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        {
            instanceBuffer->updateModified(cmdCompute);
            cmdCompute->pipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::transferWriteAccelerationStructureRead);
            cmdCompute->buildAccelerationStructure(bottomLevel, {tris}, scratchBuffer);
            cmdCompute->pipelineBarrier(
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::accelerationStructureWriteRead);
            cmdCompute->buildAccelerationStructure(topLevel, instance, scratchBuffer);
        }
        cmdCompute->end();
        magma::finish(cmdCompute, computeQueue);
    }

    void setupDescriptorSet()
    {
        setTable.topLevel = topLevel;
        descriptorSet = std::make_unique<magma::DescriptorSet>(descriptorPool, setTable,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR, hostAllocator);
    }

    void setupPipeline()
    {
        const std::vector<magma::RayTracingShaderGroup> shaderGroups{
            magma::GeneralRayTracingShaderGroup(0),
            magma::TrianglesHitRayTracingShaderGroup(1),
            magma::GeneralRayTracingShaderGroup(2)
        };
        auto layout = std::unique_ptr<magma::PipelineLayout>(new magma::PipelineLayout(
            {
                descriptorSet->getLayout(),
                swapchainDescriptorSets.front()->getLayout(),
            }, hostAllocator));
        constexpr uint32_t maxRecursionDepth = 1;
        pipeline = std::unique_ptr<magma::RayTracingPipeline>(new RayTracingPipeline(device,
            {"trace", "hit", "miss"}, shaderGroups, maxRecursionDepth, 
            std::move(layout), hostAllocator));
        shaderBindingTable.build(pipeline, commandBuffers[0], allocator);
    }

    void recordCommandBuffer(size_t index)
    {
        std::shared_ptr<magma::CommandBuffer>& cmdBuffer = commandBuffers[index];
        magma::Image *backBuffer = swapchainImageViews[index]->getImage();
        cmdBuffer->begin();
        {
            backBuffer->layoutTransition(VK_IMAGE_LAYOUT_GENERAL, cmdBuffer);
            cmdBuffer->bindPipeline(pipeline);
            cmdBuffer->bindDescriptorSets(pipeline, 0,
                {
                    descriptorSet,
                    swapchainDescriptorSets[index]
                });
            cmdBuffer->traceRays(shaderBindingTable, width, height, 1);
            backBuffer->layoutTransition(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, cmdBuffer);
        }
        cmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<TriangleApp>(new TriangleApp(entry));
}
