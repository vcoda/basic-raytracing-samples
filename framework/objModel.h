#pragma once
#include "../third-party/tinyobjloader/tiny_obj_loader.h"
#include "../third-party/magma/magma.h"
#include "../third-party/rapid/rapid.h"

class ObjMesh
{
public:
    explicit ObjMesh(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib,
        const std::vector<tinyobj::material_t>& materials, std::shared_ptr<magma::CommandBuffer> cmdBuffer,
        bool swapYZ);
    const std::shared_ptr<magma::Buffer>& getVertexBuffer() const noexcept { return vertexBuffer; }
    const std::shared_ptr<magma::Buffer>& getIndexBuffer() const noexcept { return indexBuffer; }

private:
    std::shared_ptr<magma::Buffer> vertexBuffer;
    std::shared_ptr<magma::Buffer> indexBuffer;
};

class ObjModel
{
public:
    explicit ObjModel(const std::string& fileName, std::shared_ptr<magma::CommandBuffer> cmdBuffer,
        bool swapYZ);
    uint64_t getAccelerationStructureReference() const noexcept { return bottomLevel->getReference(); }
    const std::shared_ptr<magma::StorageBuffer>& getReferenceBuffer() const noexcept { return referenceBuffer; }

private:
    std::forward_list<std::unique_ptr<ObjMesh>> meshes;
    std::shared_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
    std::shared_ptr<magma::StorageBuffer> referenceBuffer;
};
