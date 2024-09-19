#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"
#include "../framework/objModel.h"

class TextureMappingApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable: magma::DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::AccelerationStructure topLevel = 1;
        magma::descriptor::StorageBuffer bufferReferences = 2;
        magma::descriptor::CombinedImageImmutableSampler diffuseMap = 3;
        magma::descriptor::UniformBuffer normalMatrix = 4;
        MAGMA_REFLECT(view, topLevel, bufferReferences, diffuseMap, normalMatrix)
    } setTable;

    std::unique_ptr<ObjModel> model;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    magma::AccelerationStructureGeometryInstances geometryInstance;
    std::shared_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::shared_ptr<magma::StorageBuffer> bufferReferences;
    std::shared_ptr<magma::UniformBuffer<rapid::matrix>> normalMatrix;
    std::shared_ptr<magma::Sampler> bilinearSampler;
    std::shared_ptr<magma::DescriptorSet> descriptorSet;
    std::shared_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

    float zDist = 20.f;

public:
    TextureMappingApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Texture mapping"), 512, 512)
    {
        updateView();
        loadModel("12270_Frog_v1_L3.obj", true);
        createReferenceBuffer();
        createInstanceBuffer();
        buildTopLevelAccelerationStructure();
        createUniformBuffer();
        setupDescriptorSet();
        setupPipeline();
        recordCommandBuffer(Buffer::Front);
        recordCommandBuffer(Buffer::Back);
        timer->run();
    }

    void render(uint32_t bufferIndex) override
    {
        updateWorldTransform();
        submitCommandBuffer(bufferIndex);
    }

    void onMouseWheel(float delta) override
    {
        zDist += delta;
        zDist = std::max(5.f, zDist);
        updateView();
    }

    void updateView()
    {
        const rapid::vector3 eye(0.f, zDist * 0.5f, zDist);
        const rapid::vector3 center(0.f, 2.f, 0.f);
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

    void updateWorldTransform()
    {
        const rapid::matrix world = rapid::rotationY(rapid::radians(spinX/2.f));
        auto& instance = instanceBuffer->getInstance(0);
        world.store(instance.transform.matrix);
        magma::helpers::mapScoped(normalMatrix,
            [&world](rapid::matrix *normal)
            {
                *normal = rapid::transpose(rapid::inverse(world));
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
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(device, 1);
        instanceBuffer->getInstance(0).accelerationStructureReference = model->getAccelerationStructure()->getReference();
        geometryInstance = magma::AccelerationStructureGeometryInstances(instanceBuffer);
    }

    void buildTopLevelAccelerationStructure()
    {
        topLevel = std::make_shared<magma::TopLevelAccelerationStructure>(device, geometryInstance,
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
            cmdCompute->buildAccelerationStructure(topLevel, geometryInstance, scratchBuffer);
        }
        cmdCompute->end();
        magma::finish(cmdCompute, computeQueue);
    }

    void createUniformBuffer()
    {
        normalMatrix = std::make_shared<magma::UniformBuffer<rapid::matrix>>(device);
    }

    void setupDescriptorSet()
    {
        bilinearSampler = std::make_shared<magma::Sampler>(device, magma::sampler::magMinLinearMipNearestClampToEdge);
        setTable.view = viewUniforms;
        setTable.topLevel = topLevel;
        setTable.bufferReferences = bufferReferences;
        setTable.diffuseMap = {model->getMaterials().front().diffuseMap, bilinearSampler}; // Take first material
        setTable.normalMatrix = normalMatrix;
        descriptorSet = std::make_shared<magma::DescriptorSet>(descriptorPool, setTable,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    }

    void setupPipeline()
    {
        const std::vector<magma::RayTracingShaderGroup> shaderGroups{
            magma::GeneralRayTracingShaderGroup(0),
            magma::TrianglesHitRayTracingShaderGroup(1),
            magma::GeneralRayTracingShaderGroup(2)
        };
        auto layout = std::shared_ptr<magma::PipelineLayout>(new magma::PipelineLayout(
            {
                descriptorSet->getLayout(),
                swapchainDescriptorSets.front()->getLayout(),
            }));
        pipeline = std::shared_ptr<magma::RayTracingPipeline>(new RayTracingPipeline(device,
            {"trace", "hit", "miss"}, shaderGroups, 1, std::move(layout)));
        const rapid::float3 lightPos(-50, 100, 50);
        const rapid::float3 backgroundColor(0.35f, 0.53f, 0.7f);
        shaderBindingTable.addShaderRecord(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, lightPos);
        shaderBindingTable.addShaderRecord(VK_SHADER_STAGE_MISS_BIT_KHR, 2, backgroundColor);
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
            cmdBuffer->updateAccelerationStructure(topLevel, geometryInstance, scratchBuffer);
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
    return std::unique_ptr<TextureMappingApp>(new TextureMappingApp(entry));
}