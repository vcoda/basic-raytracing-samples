#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"
#include "../framework/objModel.h"

class ShaderBindingTableApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable: magma::DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::AccelerationStructure topLevel = 1;
        magma::descriptor::StorageBuffer bufferReferences = 2;
        magma::descriptor::StorageBuffer normalMatrices = 3;
        magma::descriptor::UniformBuffer lightSource = 4;
        magma::descriptor::CombinedImageImmutableSampler diffuseMap = 5;
        MAGMA_REFLECT(view, topLevel, bufferReferences, normalMatrices, lightSource, diffuseMap)
    } setTable;

    std::unique_ptr<ObjModel> model;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    magma::AccelerationStructureGeometryInstances geometryInstances;
    std::shared_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::shared_ptr<magma::StorageBuffer> bufferReferences;
    std::shared_ptr<magma::DynamicStorageBuffer> normalMatrices;
    std::shared_ptr<magma::UniformBuffer<rapid::float4a>> lightPos;
    std::shared_ptr<magma::Sampler> bilinearSampler;
    std::shared_ptr<magma::DescriptorSet> descriptorSet;
    std::shared_ptr<magma::RayTracingPipeline> pipeline;
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
        recordCommandBuffer(Buffer::Front);
        recordCommandBuffer(Buffer::Back);
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
        magma::helpers::mapScoped(viewUniforms,
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
        magma::helpers::mapScoped<rapid::matrix>(normalMatrices,
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
        model = std::make_unique<ObjModel>(fileName, cmdCompute, false, swapYZ);
    }

    void createReferenceBuffer()
    {
        std::vector<VkDeviceAddress> addresses;
        for (auto const& mesh: model->getMeshes())
        {   // Hit shader loads mesh data from these buffers
            addresses.push_back(mesh.getVertexBuffer()->getDeviceAddress());
            addresses.push_back(mesh.getIndexBuffer()->getDeviceAddress());
        }
        bufferReferences = magma::helpers::makeStorageBuffer(addresses, cmdBufferCopy);
    }

    void createInstanceBuffer()
    {
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(device, 4);
        for (uint32_t i = 0; i < instanceBuffer->getInstanceCount(); ++i)
        {
            magma::AccelerationStructureInstance& instance = instanceBuffer->getInstance(i);
            instance.instanceShaderBindingTableRecordOffset = i; // Assign hit shader
            instance.accelerationStructureReference = model->getAccelerationStructure()->getReference();
        }
        geometryInstances = magma::AccelerationStructureGeometryInstances(instanceBuffer);
    }

    void buildTopLevelAccelerationStructure()
    {
        topLevel = std::make_shared<magma::TopLevelAccelerationStructure>(device, geometryInstances,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
        scratchBuffer = allocateScratchBuffer(topLevel->getBuildScratchSize());
        cmdCompute->reset();
        cmdCompute->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        {
            instanceBuffer->updateModified(cmdCompute);
            cmdCompute->pipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::transferWriteAccelerationStructureRead);
            cmdCompute->buildAccelerationStructure(topLevel, geometryInstances, scratchBuffer);
        }
        cmdCompute->end();
        magma::finish(cmdCompute, computeQueue);
    }

    void createTransformBuffer()
    {
        normalMatrices = std::make_shared<magma::DynamicStorageBuffer>(device,
            sizeof(rapid::matrix) * instanceBuffer->getInstanceCount(), true);
    }

    void createUniformBuffer()
    {
        lightPos = std::make_shared<magma::UniformBuffer<rapid::float4a>>(device);
        magma::helpers::mapScoped(lightPos,
            [](rapid::float4a *lightPos)
            {
                lightPos->x = 0.f;
                lightPos->y = 0.f;
                lightPos->z = 100.f;
            });
    }

    void setupDescriptorSet()
    {
        bilinearSampler = std::make_shared<magma::Sampler>(device, magma::sampler::magMinLinearMipNearestClampToEdge);
        setTable.view = viewUniforms;
        setTable.topLevel = topLevel;
        setTable.bufferReferences = bufferReferences;
        setTable.normalMatrices = normalMatrices;
        setTable.lightSource = lightPos;
        setTable.diffuseMap = {model->getMaterials().front().diffuseMap, bilinearSampler};
        descriptorSet = std::make_shared<magma::DescriptorSet>(descriptorPool, setTable,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
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
        auto layout = std::shared_ptr<magma::PipelineLayout>(new magma::PipelineLayout(
            {
                descriptorSet->getLayout(),
                swapchainDescriptorSets.front()->getLayout(),
            }));
        pipeline = std::shared_ptr<magma::RayTracingPipeline>(new RayTracingPipeline(device,
            {"normal", "lambert", "diffuse", "phong", "trace", "miss"}, shaderGroups, 1, std::move(layout)));
        constexpr rapid::float3 backgroundColor(0.5f, 0.5f, 0.5f);
        shaderBindingTable.addShaderRecord(VK_SHADER_STAGE_MISS_BIT_KHR, 5, backgroundColor);
        shaderBindingTable.build(pipeline, cmdBufferCopy);
    }

    void recordCommandBuffer(uint32_t index)
    {
        auto& cmdBuffer = commandBuffers[index];
        auto& backBuffer = swapchainImageViews[index]->getImage();
        cmdBuffer->begin();
        {
            backBuffer->layoutTransition(VK_IMAGE_LAYOUT_GENERAL, cmdBuffer);
            instanceBuffer->updateWhole(cmdBuffer);
            cmdBuffer->pipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::transferWriteAccelerationStructureRead);
            cmdBuffer->updateAccelerationStructure(topLevel, geometryInstances, scratchBuffer);
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
