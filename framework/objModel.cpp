#define TINYOBJLOADER_IMPLEMENTATION
#include "objModel.h"
#include "vertex.h"
#include "packing.h"
#include "indexedVertexArray.h"

ObjMesh::ObjMesh(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib,
    const std::vector<tinyobj::material_t>& materials,
    std::shared_ptr<magma::CommandBuffer> cmdBuffer,
    bool swapYZ, bool flipV)
{
    const int i1 = swapYZ ? 2 : 1;
    const int i2 = swapYZ ? 1 : 2;
    std::vector<Vertex> vertices;
    vertices.reserve(mesh.indices.size());
    for (const auto& index: mesh.indices)
    {
        Vertex v = {};
        int vi = index.vertex_index * 3;
        v.pos.x = attrib.vertices[vi];
        v.pos.y = attrib.vertices[vi + i1];
        v.pos.z = attrib.vertices[vi + i2];
        if (index.normal_index != -1)
        {
            int ni = index.normal_index * 3;
            v.normal[0] = packSnorm(attrib.normals[ni]);
            v.normal[1] = packSnorm(attrib.normals[ni + 1]);
            v.normal[2] = packSnorm(attrib.normals[ni + 2]);
            v.normal[3] = 0;
        }
        if (index.texcoord_index != -1)
        {
            int ti = index.texcoord_index << 1;
            v.texCoord.x = attrib.texcoords[ti];
            v.texCoord.y = attrib.texcoords[ti + 1];
            if (flipV)
                v.texCoord.y = 1.f - v.texCoord.y;
        }
        vertices.push_back(v);
    }
    int face = 0;
    for (int matId: mesh.material_ids)
    {
        if (matId != -1)
        {
            const tinyobj::material_t& material = materials[matId];
            for (int i = 0, fi = face * 3; i < 3; ++i)
            {
                Vertex& v = vertices[fi + i];
                v.color[0] = packUnorm(material.diffuse[0]);
                v.color[1] = packUnorm(material.diffuse[1]);
                v.color[2] = packUnorm(material.diffuse[2]);
                v.color[3] = 255;
            }
        }
        ++face;
    }
    IndexedVertexArray<Vertex, uint32_t> indexedVertices(vertices);
    if (swapYZ)
        indexedVertices.changeWindingOrder();
    vertexBuffer = std::make_shared<magma::AccelerationStructureInputBuffer>(cmdBuffer,
        indexedVertices.getVertices().size_bytes(),
        indexedVertices.getVertices().data());
    indexBuffer = std::make_shared<magma::AccelerationStructureInputBuffer>(cmdBuffer,
        indexedVertices.getIndices().size_bytes(),
        indexedVertices.getIndices().data());
}

ObjModel::ObjModel(const std::string& fileName, std::shared_ptr<magma::CommandBuffer> cmdBuffer,
    bool swapYZ, bool flipV)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    // Load .obj model
    constexpr bool triangulate = true;
    const bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        ("../assets/" + fileName).c_str(), "../assets", triangulate);
    if (warn.length())
        std::cout << warn << std::endl;
    if (!result)
    {
        if (err.length())
            std::cout << err << std::endl;
        return;
    }
    // Create triangle mesh for each shape
    std::forward_list<magma::AccelerationStructureGeometry> geometries;
    for (const tinyobj::shape_t& shape: shapes)
    {
        std::unique_ptr<ObjMesh> mesh = std::make_unique<ObjMesh>(shape.mesh, attrib, materials, cmdBuffer, swapYZ, flipV);
        magma::AccelerationStructureGeometryTriangles triangles(
            VK_FORMAT_R32G32B32_SFLOAT, mesh->getVertexBuffer(),
            VK_INDEX_TYPE_UINT32, mesh->getIndexBuffer());
        triangles.geometry.triangles.vertexStride = sizeof(Vertex); // TODO:
        geometries.push_front(triangles);
        meshes.push_front(std::move(mesh));
    }
    // Create acceleration structure
    bottomLevel = std::make_shared<magma::BottomLevelAccelerationStructure>(
        cmdBuffer->getDevice(), geometries,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    // Allocate scratch buffer
    magma::Buffer::Initializer initializer;
    initializer.deviceAddress = true;
    std::shared_ptr<magma::Buffer> scratchBuffer = std::make_shared<magma::StorageBuffer>(
        cmdBuffer->getDevice(), bottomLevel->getBuildScratchSize(), nullptr, initializer);
    // Build acceleration structure on GPU
    cmdBuffer->reset();
    cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    {
        cmdBuffer->buildAccelerationStructure(bottomLevel, geometries, scratchBuffer);
    }
    cmdBuffer->end();
    magma::finish(cmdBuffer);
    // Create reference buffer
    vector<VkDeviceAddress> bufferAddresses;
    for (auto const& mesh: meshes)
    {   // These addresses will be used to reference buffers in the hit shader
        bufferAddresses.push_back(mesh->getVertexBuffer()->getDeviceAddress());
        bufferAddresses.push_back(mesh->getIndexBuffer()->getDeviceAddress());
    }
    referenceBuffer = std::make_shared<magma::StorageBuffer>(cmdBuffer,
        bufferAddresses.size_bytes(), bufferAddresses.data());
}
