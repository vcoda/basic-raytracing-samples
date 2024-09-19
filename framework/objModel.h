#pragma once
#include "../third-party/magma/magma.h"

struct Vertex;

namespace tinyobj
{
    struct mesh_t;
    struct material_t;
    struct attrib_t;
}

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
    void calculateVertexNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) const;

    std::shared_ptr<magma::Buffer> vertexBuffer;
    std::shared_ptr<magma::Buffer> indexBuffer;
};

struct ObjMaterial
{
    std::string name;
    std::shared_ptr<magma::ImageView> ambientMap;
    std::shared_ptr<magma::ImageView> diffuseMap;
    std::shared_ptr<magma::ImageView> specularMap;
    std::shared_ptr<magma::ImageView> bumpMap;
    std::shared_ptr<magma::ImageView> alphaMap;
    std::shared_ptr<magma::ImageView> reflectionMap;
};

class ObjModel
{
public:
    explicit ObjModel(const std::string& fileName, std::shared_ptr<magma::CommandBuffer> cmdBuffer,
        bool calculateNormals = false, bool swapYZ = false);
    const std::list<ObjMesh>& getMeshes() const noexcept { return meshes; }
    const std::list<ObjMaterial>& getMaterials() const noexcept { return materials; }
    const std::shared_ptr<magma::BottomLevelAccelerationStructure>& getAccelerationStructure() const noexcept { return bottomLevel; }

private:
    std::shared_ptr<magma::ImageView> loadTexture(const std::string& name, const std::string& directory,
        std::shared_ptr<magma::CommandBuffer> cmdBuffer);

    std::list<ObjMesh> meshes;
    std::list<ObjMaterial> materials;
    std::map<std::string, std::shared_ptr<magma::ImageView>> textureCache;
    std::shared_ptr<magma::BottomLevelAccelerationStructure> bottomLevel;
};
