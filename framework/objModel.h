#pragma once
#include "../third-party/tinyobjloader/tiny_obj_loader.h"
#include "../third-party/magma/magma.h"
#include "../third-party/rapid/rapid.h"
#include "vertex.h"
#include "utilities.h"

class ObjMesh
{
public:
    explicit ObjMesh(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib,
        const std::vector<tinyobj::material_t>& materials,
        std::shared_ptr<magma::CommandBuffer> cmdBuffer,
        bool calculateNormals, bool swapYZ);
    const std::shared_ptr<magma::Buffer>& getVertexBuffer() const noexcept { return vertexBuffer; }
    const std::shared_ptr<magma::Buffer>& getIndexBuffer() const noexcept { return indexBuffer; }

private:
    void calculateVertexNormals(vector<Vertex>& vertices, const vector<uint32_t>& indices) const;

    std::shared_ptr<magma::Buffer> vertexBuffer;
    std::shared_ptr<magma::Buffer> indexBuffer;
};

class ObjModel
{
public:
    explicit ObjModel(const std::string& fileName, std::shared_ptr<magma::CommandBuffer> cmdBuffer,
        bool calculateNormals = false, bool swapYZ = false);
    const std::list<ObjMesh>& getMeshes() const noexcept { return meshes; }
    uint64_t getAccelerationStructureReference() const noexcept { return bottomLevel->getReference(); }

private:
    std::list<ObjMesh> meshes;
    std::shared_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
};
