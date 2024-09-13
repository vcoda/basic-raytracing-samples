#define TINYOBJLOADER_IMPLEMENTATION
#include "../third-party/tinyobjloader/tiny_obj_loader.h"
#include "../framework/vulkanRtApp.h"
#include "../framework/rayTracingPipeline.h"

class MeshApp : public VulkanRayTracingApp
{
    struct DescriptorSetTable: magma::DescriptorSetTable
    {
        magma::descriptor::UniformBuffer view = 0;
        magma::descriptor::AccelerationStructure topLevel = 1;
        magma::descriptor::StorageBuffer vertices = 2;
        magma::descriptor::UniformBuffer normalMatrix = 3;
        MAGMA_REFLECT(view, topLevel, vertices, normalMatrix)
    } setTable;

    magma::AccelerationStructureGeometryTriangles geometry;
    magma::AccelerationStructureGeometryInstances geometryInstance;
    std::shared_ptr<magma::AccelerationStructureInputBuffer> vertexBuffer;
    std::unique_ptr<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>> instanceBuffer;
    std::shared_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
    std::shared_ptr<magma::TopLevelAccelerationStructure> topLevel;
    std::shared_ptr<magma::UniformBuffer<rapid::matrix>> normalMatrix;
    std::shared_ptr<magma::DescriptorSet> descriptorSet;
    std::shared_ptr<magma::RayTracingPipeline> pipeline;
    magma::ShaderBindingTable shaderBindingTable;

public:
    MeshApp(const AppEntry& entry):
        VulkanRayTracingApp(entry, TEXT("Mesh"), 512, 512)
    {
        setupView();
        loadMesh("rat.obj");
        createAccelerationStructures();
        buildAccelerationStructures();
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

    void setupView()
    {
        const rapid::vector3 eye(0.f, 80.f, 150.f);
        const rapid::vector3 center(0.f, 40.f, 0.f);
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
        constexpr float speed = 0.05f;
        const float step = timer->millisecondsElapsed() * speed;
        static float angle = 0.f;
        angle += step;
        const rapid::matrix world = rapid::rotationY(rapid::radians(angle));
        auto& instance = instanceBuffer->getInstance(0);
        world.store(instance.transform.matrix);
        magma::helpers::mapScoped(normalMatrix,
            [&world](rapid::matrix *normal)
            {
                *normal = rapid::transpose(rapid::inverse(world));
            });
    }

    void loadMesh(const std::string& fileName)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, error;
        bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error,
            ("../assets/meshes/" + fileName).c_str(), "../assets/meshes", true);
        MAGMA_ASSERT(result);
        if (warn.length())
            std::cout << warn << std::endl;
        if (error.length())
            std::cerr << error << std::endl;
        std::vector<rapid::float4> vertices;
        const tinyobj::mesh_t& mesh = shapes.front().mesh;
        vertices.reserve(mesh.indices.size());
        for (const auto& index: mesh.indices)
        {
            const int offset = index.vertex_index * 3;
            vertices.emplace_back(
                attrib.vertices[offset],
                attrib.vertices[offset + 1],
                attrib.vertices[offset + 2],
                1.f);
        }
        vertexBuffer = magma::helpers::makeInputBuffer(vertices, cmdBufferCopy);
        geometry = magma::AccelerationStructureGeometryTriangles(VK_FORMAT_R32G32B32A32_SFLOAT, vertexBuffer);
        instanceBuffer = std::make_unique<magma::AccelerationStructureInstanceBuffer<magma::AccelerationStructureInstance>>(device, 1);
        geometryInstance = magma::AccelerationStructureGeometryInstances(instanceBuffer);
    }

    void createAccelerationStructures()
    {
        bottomLevel = std::make_shared<magma::BottomLevelAccelerationStructure>(device,
            std::forward_list<magma::AccelerationStructureGeometry>{geometry},
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
            cmdCompute->buildAccelerationStructure(bottomLevel, {geometry}, scratchBuffer);
            cmdCompute->pipelineBarrier(
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                magma::barrier::memory::accelerationStructureWriteRead);
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
        setTable.view = viewUniforms;
        setTable.topLevel = topLevel;
        setTable.vertices = vertexBuffer;
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
        // Light pos
        shaderBindingTable.addShaderRecord(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, rapid::float3(-100, 200, 100));
        // Background color
        shaderBindingTable.addShaderRecord(VK_SHADER_STAGE_MISS_BIT_KHR, 2, rapid::float3(0.35f, 0.53f, 0.7f));
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
    return std::unique_ptr<MeshApp>(new MeshApp(entry));
}
