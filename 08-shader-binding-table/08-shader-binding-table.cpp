#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"
#include "../framework/objModel.h"
#include "../framework/utilities.h"

class ShaderBindingTableApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::AccelerationStructure topLevel = 1;
        magma::descriptor::StorageBuffer bufferReferences = 2;
        magma::descriptor::StorageBuffer normalMatrices = 3;
        magma::descriptor::UniformBuffer lightSource = 4;
        magma::descriptor::CombinedImageImmutableSampler diffuseMap = 5;
    } setTable;

    std::unique_ptr<ObjModel> model;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    magma::AccelerationStructureInstances instances;
    std::unique_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::unique_ptr<magma::StorageBuffer> bufferReferences;
    std::unique_ptr<magma::Buffer> scratchBuffer;
    std::unique_ptr<magma::DynamicStorageBuffer> normalMatrices;
    std::unique_ptr<magma::UniformBuffer<rapid::float4a>> lightPos;
    std::unique_ptr<magma::Sampler> bilinearSampler;
    std::unique_ptr<magma::DescriptorSet> descriptorSet;
    std::unique_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

public:
    ShaderBindingTableApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Shader binding table"), 512, 512)
    {
        setupView();
        loadModel("ball/10487_basketball_v1_3dmax2011_it2.obj", true);
        createReferenceBuffer();
        createInstanceBuffer();
        buildTopLevelAccelerationStructure();
        createTransformBuffer();
        createUniformBuffer();
        setupDescriptorSet();
        setupPipeline();
        for (size_t i = 0; i < commandBuffers.size(); ++i)
            recordCommandBuffer(i);
        timer->run();
    }

    void render(uint32_t bufferIndex) override
    {
        updateWorldTransforms();
        submitCommandBuffer(bufferIndex);
    }

    void setupView()
    {
        const rapid::vector3 eye(0.f, 0.f, 150.f);
        const rapid::vector3 center(0.f, 0.f, 0.f);
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
                data->projInv = rapid::inverse(rapid::negateY(proj));
                data->viewProjInv = data->projInv * rapid::matrix3(data->viewInv);
            });
    }

    void updateWorldTransforms()
    {
        const rapid::matrix pitch = rapid::rotationX(rapid::radians(spinY/2.f));
        const rapid::matrix yaw = rapid::rotationY(rapid::radians(spinX/2.f));
        const rapid::matrix rotation = pitch * yaw;
        magma::map<rapid::matrix>(normalMatrices,
            [&rotation, this](rapid::matrix *normalMatrices)
        {
            constexpr rapid::float2 offsets[4] = {
                {-30.f, 30.f},
                {30.f, 30.f},
                {-30.f, -30.f},
                {30.f, -30.f}
            };
            for (uint32_t i = 0; i < instanceBuffer->getInstanceCount(); ++i)
            {
                const rapid::matrix translation = rapid::translation(offsets[i].x, offsets[i].y, 0.f);
                const rapid::matrix world = rotation * translation;
                world.store(instanceBuffer->getInstance(i).transform.matrix);
                normalMatrices[i] = rapid::transpose(rapid::inverse(world));
            }
        });
    }

    void loadModel(const std::string& fileName, bool swapYZ)
    {
        model = std::make_unique<ObjModel>(fileName, cmdCompute, allocator, false, swapYZ);
    }

    void createReferenceBuffer()
    {
        vector<VkDeviceAddress> addresses;
        for (auto const& mesh: model->getMeshes())
        {   // Hit shader loads mesh data from these buffers
            addresses.push_back(mesh.getVertexBuffer()->getDeviceAddress());
            addresses.push_back(mesh.getIndexBuffer()->getDeviceAddress());
        }
        bufferReferences = utilities::makeStorageBuffer(addresses, cmdBufferCopy, allocator);
    }

    void createInstanceBuffer()
    {
        constexpr uint32_t instanceCount = 4;
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(device, instanceCount, allocator);
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            magma::AccelerationStructureInstance& instance = instanceBuffer->getInstance(i);
            instance.instanceShaderBindingTableRecordOffset = i; // Assign hit shader
            instance.accelerationStructureReference = model->getAccelerationStructure()->getReference();
        }
        instances = magma::AccelerationStructureInstances(instanceBuffer.get());
    }

    void buildTopLevelAccelerationStructure()
    {
        topLevel = std::make_unique<magma::TopLevelAccelerationStructure>(device, instances,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
            allocator);
        scratchBuffer = allocateScratchBuffer(topLevel->getBuildScratchSize());
        cmdCompute->reset();
        cmdCompute->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        {
            instanceBuffer->updateModified(cmdCompute);
            cmdCompute->pipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::transferWriteAccelerationStructureRead);
            cmdCompute->buildAccelerationStructure(topLevel, instances, scratchBuffer);
        }
        cmdCompute->end();
        magma::finish(cmdCompute, computeQueue);
    }

    void createTransformBuffer()
    {
        constexpr bool stagedPool = true;
        normalMatrices = std::make_unique<magma::DynamicStorageBuffer>(device,
            sizeof(rapid::matrix) * instanceBuffer->getInstanceCount(), 
            stagedPool, allocator);
    }

    void createUniformBuffer()
    {
        lightPos = std::make_unique<magma::UniformBuffer<rapid::float4a>>(device, allocator);
        magma::map(lightPos,
            [](rapid::float4a *lightPos)
            {
                lightPos->x = 0.f;
                lightPos->y = 0.f;
                lightPos->z = 100.f;
            });
    }

    void setupDescriptorSet()
    {
        bilinearSampler = std::make_unique<magma::Sampler>(device, magma::sampler::magMinLinearMipNearestClampToEdge);
        setTable.view = viewUniforms;
        setTable.topLevel = topLevel;
        setTable.bufferReferences = bufferReferences;
        setTable.normalMatrices = normalMatrices;
        setTable.lightSource = lightPos;
        setTable.diffuseMap = {model->getMaterials().front().diffuseMap, bilinearSampler};
        descriptorSet = std::make_unique<magma::DescriptorSet>(descriptorPool, setTable,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, hostAllocator);
    }

    void setupPipeline()
    {
        const std::vector<magma::RayTracingShaderGroup> shaderGroups{
            magma::TrianglesHitRayTracingShaderGroup(0),
            magma::TrianglesHitRayTracingShaderGroup(1),
            magma::TrianglesHitRayTracingShaderGroup(2),
            magma::TrianglesHitRayTracingShaderGroup(3),
            magma::GeneralRayTracingShaderGroup(4),
            magma::GeneralRayTracingShaderGroup(5)
        };
        auto layout = std::unique_ptr<magma::PipelineLayout>(new magma::PipelineLayout(
            {
                descriptorSet->getLayout(),
                swapchainDescriptorSets.front()->getLayout(),
            }, hostAllocator));
        constexpr uint32_t maxRecursionDepth = 1;
        pipeline = std::unique_ptr<magma::RayTracingPipeline>(new RayTracingPipeline(device,
            {"normal", "lambert", "diffuse", "phong", "trace", "miss"}, shaderGroups, maxRecursionDepth,
            std::move(layout), hostAllocator));
        constexpr rapid::float3 backgroundColor(0.5f, 0.5f, 0.5f);
        shaderBindingTable.addShaderRecord(VK_SHADER_STAGE_MISS_BIT_KHR, 5, backgroundColor);
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
            cmdBuffer->updateAccelerationStructure(topLevel, instances, scratchBuffer);
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
    return std::unique_ptr<ShaderBindingTableApp>(new ShaderBindingTableApp(entry));
}
