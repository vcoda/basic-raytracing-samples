#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"
#include "../framework/image.h"
#include "../framework/utilities.h"

std::unique_ptr<magma::UniqueImageView> createDebugCheckerboard(magma::lent_ptr<magma::CommandBuffer> cmdBuffer, uint32_t width, uint32_t height,
    std::shared_ptr<magma::Allocator> allocator = nullptr);
std::unique_ptr<magma::UniqueImageView> createDebugMipmap(magma::lent_ptr<magma::CommandBuffer> cmdBuffer,
    std::shared_ptr<magma::Allocator> allocator = nullptr);

class RayDifferentialsApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::UniformBuffer parameters = 1;
        magma::descriptor::AccelerationStructure topLevel = 2;
        magma::descriptor::CombinedImageSampler checkerboard = 3;
        magma::descriptor::CombinedImageSampler debugMipmap = 4;
        magma::descriptor::StorageBuffer vertices = 5;
        magma::descriptor::StorageBuffer indices = 6;
    } setTable;

    struct alignas(16) Parameters
    {
        VkBool32 useFiltering;
    };

    magma::AccelerationStructureIndexedTriangles tris;
    magma::AccelerationStructureInstances instance;
    std::unique_ptr<magma::ImageView> debugCheckerboard;
    std::unique_ptr<magma::ImageView> debugMipmap;
    std::unique_ptr<magma::Sampler> trilinearSampler;
    std::unique_ptr<magma::AccelerationStructureInputBuffer> vertexBuffer;
    std::unique_ptr<magma::AccelerationStructureInputBuffer> indexBuffer;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    std::unique_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
    std::unique_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::unique_ptr<magma::Buffer> scratchBuffer;
    std::unique_ptr<magma::UniformBuffer<Parameters>> parameters;
    std::unique_ptr<magma::DescriptorSet> descriptorSet;
    std::unique_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

    rapid::float3 camPos;
    rapid::matrix proj;

public:
    RayDifferentialsApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Ray differentials"), 512, 512),
        camPos(0.f, 0.f, 3.99f)
    {
        setProjection();
        createGeometry();
        createAccelerationStructures();
        buildAccelerationStructures();
        createTextures(1024, 2048);
        createUniformBuffer();
        setupDescriptorSet();
        setupPipeline();
        for (size_t i = 0; i < commandBuffers.size(); ++i)
            recordCommandBuffer(i);
        timer->run();
    }

    void render(uint32_t bufferIndex) override
    {
        updateView();
        submitCommandBuffer(bufferIndex);
    }

    void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Space:
            magma::map(parameters, [](auto *p) {
                p->useFiltering = !p->useFiltering;
            });
            break;
        case AppKey::Left: camPos.x -= 0.05f; break;
        case AppKey::Right: camPos.x += 0.05f; break;
        case AppKey::Up: camPos.y -= 0.05f; break;
        case AppKey::Down: camPos.y += 0.05f; break;
        }
        camPos.x = std::clamp(camPos.x, -1.99f, 1.99f);
        camPos.y = std::clamp(camPos.y, -1.99f, 1.99f);
        VulkanRayTracingApp::onKeyDown(key, repeat, flags);
    }

    void setProjection()
    {
        constexpr float fov = rapid::radians(90.f);
        const float aspect = width/(float)height;
        constexpr float zn = 0.1f, zf = 10.f;
        proj = rapid::perspectiveFovRH(fov, aspect, zn, zf);
        magma::map(viewUniforms, [this](View *data) {
            data->projInv = rapid::inverse(proj);
        });
    }

    void updateView()
    {
        static float angle = 0.f;
        angle += timer->millisecondsElapsed() * 0.001f;
        const float x = sinf(angle) * 0.3f;
        const float y = cosf(angle) * 0.3f;
        const rapid::vector3 eye(camPos);
        const rapid::vector3 at(x, y, -4.f);
        const rapid::vector3 up(0.f, 1.f, 0.f);
        rapid::matrix view = rapid::lookAtRH(eye, at, up);
        magma::map(viewUniforms,
            [&view](View *data)
            {
                data->viewInv = rapid::inverse(view);
                data->viewProjInv = data->projInv * rapid::matrix3(data->viewInv);
            });
    }

    void createGeometry()
    {
        constexpr float w = 2.f, h = 2.f, d = 4.f;
        // pos.xyz, u, normal.xyz, v
        const float vertices[] = {
            -w, -h, d, 0, -1, 0, 0, 0,
            -w, h, d, 1, -1, 0, 0, 0,
            -w, -h, -d, 0, -1, 0, 0, 1,
            -w, h, -d, 1, -1, 0, 0, 1,
            w, -h, -d, 0, 1, 0, 0, 0,
            w, h, -d, 1, 1, 0, 0, 0,
            w, -h, d, 0, 1, 0, 0, 1,
            w, h, d, 1, 1, 0, 0, 1,
            -w, -h,  d, 0, 0, -1, 0, 0,
            -w, -h, -d, 0, 0, -1, 0, 1,
            w, -h,  d, 1, 0, -1, 0, 0,
            w, -h, -d, 1, 0, -1, 0, 1,
            -w, h, -d, 0, 0, 1, 0, 0,
            -w, h, d, 0, 0, 1, 0, 1,
            w, h, -d, 1, 0, 1, 0, 0,
            w, h, d, 1, 0, 1, 0, 1,
            -w, -h, -d, 0, 0, 0, -1, 0,
            -w, h, -d, 1, 0, 0, -1, 0,
            w, -h, -d, 0, 0, 0, -1, 1,
            w, h, -d, 1, 0, 0, -1, 1,
            w, -h, d, 0, 0, 0, 1, 0,
            w, h, d, 1, 0, 0, 1, 0,
            -w, -h, d, 0, 0, 0, 1, 1,
            -w, h, d, 1, 0, 0, 1, 1
        };
        constexpr uint32_t indices[] = {
            0, 1, 2, 2, 1, 3,
            4, 5, 6, 6, 5, 7,
            8, 9, 10, 10, 9, 11,
            12, 13, 14, 14, 13, 15,
            16, 17, 18, 18, 17, 19,
            20, 21, 22, 22, 21, 23
        };
        vertexBuffer = utilities::makeInputBuffer(vertices, cmdBufferCopy, allocator);
        indexBuffer = utilities::makeInputBuffer(indices, cmdBufferCopy, allocator);
        constexpr VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        constexpr VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        const magma::DeviceFeatures::FormatFeatures features = physicalDevice->features()->supportsFormatFeatures(
            vertexFormat, VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR);
        MAGMA_ASSERT(features.buffer);
        if (features.buffer)
        {
            tris = magma::AccelerationStructureIndexedTriangles(
                vertexFormat, vertexBuffer.get(), indexType, indexBuffer.get(), sizeof(rapid::float4) * 2);
        }
    }

    void createAccelerationStructures()
    {
        bottomLevel = std::make_unique<magma::BottomLevelAccelerationStructure>(device,
            std::list<magma::AccelerationStructureGeometry>{tris},
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            allocator);
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(
            device, 1, allocator);
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

    void createTextures(uint32_t width, uint32_t height)
    {
        debugCheckerboard = createDebugCheckerboard(cmdImageCopy, width, height, allocator);
        debugMipmap = createDebugMipmap(cmdImageCopy, allocator);
        trilinearSampler = std::make_unique<magma::Sampler>(device, magma::sampler::magMinMipLinearClampToEdge);
    }

    void createUniformBuffer()
    {
        parameters = std::make_unique<magma::UniformBuffer<Parameters>>(device, allocator);
        magma::map(parameters, [](auto *parameters) {
            parameters->useFiltering = false;
        });
    }

    void setupDescriptorSet()
    {
        setTable.view = viewUniforms;
        setTable.parameters = parameters;
        setTable.topLevel = topLevel;
        setTable.checkerboard = {debugCheckerboard, trilinearSampler};
        setTable.debugMipmap = {debugMipmap, trilinearSampler};
        setTable.vertices = vertexBuffer;
        setTable.indices = indexBuffer;
        descriptorSet = std::make_unique<magma::DescriptorSet>(descriptorPool, setTable,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, hostAllocator);
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
    return std::unique_ptr<RayDifferentialsApp>(new RayDifferentialsApp(entry));
}
