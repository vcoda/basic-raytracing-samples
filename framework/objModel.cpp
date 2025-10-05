#define TINYOBJLOADER_IMPLEMENTATION
#include "../third-party/tinyobjloader/tiny_obj_loader.h"
#include "../third-party/rapid/rapid.h"
#include "objModel.h"
#include "vertex.h"
#include "packing.h"
#include "indexedVertexArray.h"
#include "image.h"

ObjMesh::ObjMesh(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib,
    const std::vector<tinyobj::material_t>& materials,
    const std::unique_ptr<magma::CommandBuffer>& cmdBuffer,
    std::shared_ptr<magma::Allocator> allocator,
    bool calculateNormals, bool swapYZ)
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
            v.normal[1] = packSnorm(attrib.normals[ni + i1]);
            v.normal[2] = packSnorm(attrib.normals[ni + i2]);
            v.normal[3] = 0;
        }
        if (index.texcoord_index != -1)
        {
            int ti = index.texcoord_index << 1;
            v.texCoord.x = attrib.texcoords[ti];
            v.texCoord.y = 1.f - attrib.texcoords[ti + 1];
        }
        vertices.push_back(v);
    }
    if (!materials.empty())
    {
        int face = 0;
        for (int matId: mesh.material_ids)
        {
            if (matId < 0)
                matId = static_cast<int>(materials.size()) - 1; // default
            const tinyobj::material_t& material = materials[matId];
            for (int i = 0, fi = face * 3; i < 3; ++i)
            {
                Vertex& v = vertices[fi + i];
                v.color[0] = packUnorm(material.diffuse[0]);
                v.color[1] = packUnorm(material.diffuse[1]);
                v.color[2] = packUnorm(material.diffuse[2]);
                v.color[3] = 255;
                v.matId = static_cast<uint32_t>(matId);
            }
            ++face;
        }
    }
    IndexedVertexArray<Vertex, uint32_t> indexedVertices(vertices);
    if (swapYZ)
        indexedVertices.changeWindingOrder();
    if (calculateNormals)
        calculateVertexNormals(indexedVertices.getVertices(), indexedVertices.getIndices());
    vertexBuffer = std::make_unique<magma::AccelerationStructureInputBuffer>(cmdBuffer,
        indexedVertices.getVertices().size_bytes(),
        indexedVertices.getVertices().data(), allocator);
    indexBuffer = std::make_unique<magma::AccelerationStructureInputBuffer>(cmdBuffer,
        indexedVertices.getIndices().size_bytes(),
        indexedVertices.getIndices().data(), std::move(allocator));
}

void ObjMesh::calculateVertexNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) const
{
    std::vector<rapid::vector3> normals(vertices.size(), XMVectorZero());
    const std::size_t numTriangles = indices.size() / 3;
    for (std::size_t i = 0; i < numTriangles; ++i)
    {
        std::size_t offset = i * 3;
        uint32_t i0 = indices[offset];
        uint32_t i1 = indices[offset + 1];
        uint32_t i2 = indices[offset + 2];
        rapid::vector3 v0(vertices[i0].pos);
        rapid::vector3 v1(vertices[i1].pos);
        rapid::vector3 v2(vertices[i2].pos);
        rapid::vector3 normal = (v1 - v0).cross(v2 - v0);
        normals[i0] += normal;
        normals[i1] += normal;
        normals[i2] += normal;
    }
    uint32_t i = 0;
    for (auto& v: normals)
    {
        rapid::float3 normal;
        v.normalized().store(&normal);
        vertices[i].normal[0] = packSnorm(normal.x);
        vertices[i].normal[1] = packSnorm(normal.y);
        vertices[i].normal[2] = packSnorm(normal.z);
        ++i;
    }
}

ObjModel::ObjModel(const std::string& fileName, const std::unique_ptr<magma::CommandBuffer>& cmdBuffer,
    std::shared_ptr<magma::Allocator> allocator, bool calculateNormals /* false */, bool swapYZ /* false */)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::string directory;
    size_t lastSlash = fileName.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        directory = fileName.substr(0, lastSlash);
    // Load .obj model
    constexpr bool triangulate = true;
    const bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        ("../assets/meshes/" + fileName).c_str(),
        ("../assets/meshes/" + directory).c_str(), triangulate);
    if (warn.length())
        std::cout << warn;
    if (!result)
    {
        if (err.length())
            std::cout << err;
        return;
    }
    // Create triangle geometry for each shape
    std::list<magma::AccelerationStructureGeometry> geometries;
    for (const tinyobj::shape_t& shape: shapes)
    {
        ObjMesh mesh(shape.mesh, attrib, materials, cmdBuffer, allocator, calculateNormals, swapYZ);
        magma::AccelerationStructureIndexedTriangles tris(
            VK_FORMAT_R32G32B32_SFLOAT, mesh.getVertexBuffer().get(),
            VK_INDEX_TYPE_UINT32, mesh.getIndexBuffer().get(),
            sizeof(Vertex));
        geometries.push_back(tris);
        meshes.emplace_back(std::move(mesh));
    }
    // Create BLAS for all geometries
    bottomLevel = std::make_shared<magma::BottomLevelAccelerationStructure>(cmdBuffer->getDevice(),
        geometries,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        allocator);
    // Allocate scratch buffer
    std::unique_ptr<magma::Buffer> scratchBuffer = std::make_unique<magma::AccelerationStructureStorageBuffer>(
        cmdBuffer->getDevice(), bottomLevel->getBuildScratchSize(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, allocator);
    if (cmdBuffer->reset())
    {
        if (cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        {   // Build BLAS on device
            cmdBuffer->buildAccelerationStructure(bottomLevel, geometries, scratchBuffer);
            cmdBuffer->end();
            // Block until execution is complete
            magma::finish(cmdBuffer);
        }
    }
    // Load materials
    textureCache["blank"] = loadBlankImage(cmdBuffer, allocator);
    for (const tinyobj::material_t& mat: materials)
    {
        ObjMaterial material;
        material.ambientMap = loadTexture(mat.ambient_texname, directory, cmdBuffer, allocator);
        material.diffuseMap = loadTexture(mat.diffuse_texname, directory, cmdBuffer, allocator);
        material.specularMap = loadTexture(mat.specular_texname, directory, cmdBuffer, allocator);
        material.bumpMap = loadTexture(mat.bump_texname, directory, cmdBuffer, allocator);
        material.alphaMap = loadTexture(mat.alpha_texname, directory, cmdBuffer, allocator);
        material.reflectionMap = loadTexture(mat.reflection_texname, directory, cmdBuffer, allocator);
        this->materials.push_back(material);
    }
}

std::shared_ptr<magma::ImageView> ObjModel::loadTexture(const std::string& name, const std::string& directory,
    const std::unique_ptr<magma::CommandBuffer>& cmdBuffer, std::shared_ptr<magma::Allocator> allocator)
{
    if (name.empty())
        return textureCache["blank"];
    // Lookup texture cache
    auto it = textureCache.find(name);
    if (it != textureCache.end())
        return it->second;
    std::unique_ptr<magma::ImageView> texture = loadImage("../assets/meshes/" + directory + "/" + name, cmdBuffer, std::move(allocator));
    if (texture)
        return textureCache[name] = std::move(texture);
    return textureCache["blank"];
}
