#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"
#include "../framework/utilities.h"

class TransformApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::AccelerationStructure topLevel = 1;
    } setTable;

    magma::AccelerationStructureTriangles tris;
    magma::AccelerationStructureInstances instance;
    std::unique_ptr<magma::AccelerationStructureInputBuffer> vertexBuffer;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    std::unique_ptr<magma::Buffer> scratchBuffer;
    std::unique_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
    std::unique_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::unique_ptr<magma::DescriptorSet> descriptorSet;
    std::unique_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

public:
    TransformApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Transform"), 512, 512)
    {
        setupView();
        createGeometry();
        createAccelerationStructures();
        buildAccelerationStructures();
        setupDescriptorSet();
        setupPipeline();
        for (size_t i = 0; i < commandBuffers.size(); ++i)
            recordCommandBuffer(i);
        timer->run();
    }

    void render(uint32_t bufferIndex) override
    {
        updateWorldTransform();
        submitCommandBuffer(bufferIndex);
    }

    void setupView()
    {
        const rapid::vector3 eye(0.f, 0.f, -2.3f);
        const rapid::vector3 center(0.f);
        const rapid::vector3 up(0.f, 1.f, 0.f);
        constexpr float fov = rapid::radians(45.f);
        const float aspect = width/(float)height;
        constexpr float zn = 0.1f, zf = 1.f;
        const rapid::matrix view = rapid::lookAtRH(eye, center, up);
        const rapid::matrix proj = rapid::perspectiveFovRH(fov, aspect, zn, zf);
        magma::map(viewUniforms,
            [&view, &proj](View *data)
            {
                data->viewInv = rapid::inverse(view);
                data->projInv = rapid::inverse(proj);
                data->viewProjInv = data->projInv * rapid::matrix3(data->viewInv);
            });
    }

    void updateWorldTransform()
    {
        constexpr float speed = 0.05f;
        const float step = timer->millisecondsElapsed() * speed;
        static float angle = 0.f;
        angle += step;
        const rapid::matrix world = rapid::rotationY(rapid::radians(angle));
        auto& instance = instanceBuffer->getInstance(0);
        world.store(instance.transform.matrix);
    }

    void createGeometry()
    {
        const magma::vt::Pos3f vertices[] = {
            { 0.0f,-0.3f, 0.f},
            {-0.6f, 0.3f, 0.f},
            { 0.6f, 0.3f, 0.f}
        };
        vertexBuffer = utilities::makeInputBuffer(vertices, cmdBufferCopy, allocator);
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
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
            allocator);
    }

    void buildAccelerationStructures()
    {
        const VkDeviceSize maxSize = std::max(bottomLevel->getBuildScratchSize(), topLevel->getBuildScratchSize());
        scratchBuffer = allocateScratchBuffer(maxSize);
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
        setTable.view = viewUniforms;
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
        shaderBindingTable.build(pipeline, cmdBufferCopy, allocator);
    }

    void recordCommandBuffer(size_t index)
    {
        std::shared_ptr<magma::CommandBuffer>& cmdBuffer = commandBuffers[index];
        magma::Image *backBuffer = swapchainImageViews[index]->getImage();
        cmdBuffer->begin();
        {
            backBuffer->layoutTransition(VK_IMAGE_LAYOUT_GENERAL, cmdBuffer);
            instanceBuffer->updateWhole(cmdBuffer);
            cmdBuffer->pipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::transferWriteAccelerationStructureRead);
            cmdBuffer->updateAccelerationStructure(topLevel, instance, scratchBuffer);
            cmdBuffer->pipelineBarrier(
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                magma::barrier::memory::accelerationStructureWriteShaderRead);
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
    return std::unique_ptr<TransformApp>(new TransformApp(entry));
}
