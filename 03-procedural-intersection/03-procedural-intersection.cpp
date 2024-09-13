#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"
#include "../framework/utilities.h"

class ProceduralIntersectionApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable: magma::DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::AccelerationStructure topLevel = 1;
        MAGMA_REFLECT(view, topLevel)
    } setTable;

    magma::AccelerationStructureGeometryAabbs aabbGeometry;
    magma::AccelerationStructureGeometryInstances geometryInstance;
    std::unique_ptr<magma::AccelerationStructureInputBuffer> aabbBuffer;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    std::shared_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
    std::shared_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::shared_ptr<magma::DescriptorSet> descriptorSet;
    std::shared_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

public:
    ProceduralIntersectionApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Procedural intersection"), 512, 512)
    {
        setupView();
        createBoundingBox();
        createAccelerationStructures();
        buildAccelerationStructures();
        setupDescriptorSet();
        setupPipeline();
        recordCommandBuffer(Buffer::Front);
        recordCommandBuffer(Buffer::Back);
        timer->run();
    }

    void render(uint32_t bufferIndex) override
    {
        submitCommandBuffer(bufferIndex);
    }

    void setupView()
    {
        const rapid::vector3 eye(0.f, 0.f, 5.f);
        const rapid::vector3 center(0.f);
        const rapid::vector3 up(0.f, 1.f, 0.f);
        constexpr float fov = rapid::radians(45.f);
        const float aspect = width/(float)height;
        constexpr float zn = 1.f, zf = 2.f;
        const rapid::matrix view = rapid::lookAtRH(eye, center, up);
        rapid::matrix proj = rapid::perspectiveFovRH(fov, aspect, zn, zf);
        magma::helpers::mapScoped(viewUniforms,
            [&view, &proj](View *data)
            {
                data->viewInv = rapid::inverse(view);
                data->projInv = rapid::inverse(proj);
                data->viewProjInv = data->projInv * rapid::matrix3(data->viewInv);
            });
    }

    void createBoundingBox()
    {
        alignas(16) VkAabbPositionsKHR aabb = {
            -1, -1, -1,
            1, 1, 1
        };
        aabbBuffer = magma::helpers::makeInputBuffer(aabb, cmdBufferCopy);
        aabbGeometry = magma::AccelerationStructureGeometryAabbs(aabbBuffer);
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(device, 1);
        geometryInstance = magma::AccelerationStructureGeometryInstances(instanceBuffer);
    }

    void createAccelerationStructures()
    {
        bottomLevel = std::make_shared<magma::BottomLevelAccelerationStructure>(device,
            std::forward_list<magma::AccelerationStructureGeometry>{aabbGeometry},
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
        instanceBuffer->getInstance(0).accelerationStructureReference = bottomLevel->getReference();
        topLevel = std::make_shared<magma::TopLevelAccelerationStructure>(device,
            geometryInstance,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
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
            cmdCompute->buildAccelerationStructure(bottomLevel, {aabbGeometry}, scratchBuffer);
            cmdCompute->pipelineBarrier(
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::accelerationStructureWriteRead);
            cmdCompute->buildAccelerationStructure(topLevel, geometryInstance, scratchBuffer);
        }
        cmdCompute->end();
        magma::finish(cmdCompute, computeQueue);
    }

    void setupDescriptorSet()
    {
        setTable.view = viewUniforms;
        setTable.topLevel = topLevel;
        descriptorSet = std::make_shared<magma::DescriptorSet>(descriptorPool, setTable, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    }

    void setupPipeline()
    {
        const std::vector<magma::RayTracingShaderGroup> shaderGroups{
            magma::GeneralRayTracingShaderGroup(0),
            magma::GeneralRayTracingShaderGroup(1),
            magma::ProceduralHitRayTracingShaderGroup(3, 2),
        };
        auto layout = std::shared_ptr<magma::PipelineLayout>(new magma::PipelineLayout(
            {
                descriptorSet->getLayout(),
                swapchainDescriptorSets.front()->getLayout(),
            }));
        constexpr uint32_t maxRecursionDepth = 1;
        pipeline = std::shared_ptr<magma::RayTracingPipeline>(new RayTracingPipeline(device,
            {"trace", "miss", "hit", "raySphere"}, shaderGroups, maxRecursionDepth, std::move(layout)));
        shaderBindingTable.build(pipeline, cmdBufferCopy);
    }

    void recordCommandBuffer(uint32_t index)
    {
        auto& cmdBuffer = commandBuffers[index];
        auto& backBuffer = swapchainImageViews[index]->getImage();
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
    return std::unique_ptr<ProceduralIntersectionApp>(new ProceduralIntersectionApp(entry));
}
