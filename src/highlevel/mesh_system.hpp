#pragma once

#include "berkeley_gfx.hpp"
#include "bbox.hpp"

#include <vulkan/vulkan.hpp>

namespace BG::MeshSystem
{

  struct Vertex
  {
    glm::vec3 pos;
    int materialIndex;
    glm::vec3 normal;
    glm::vec2 uv0;
    glm::vec2 uv1;
  };

  class Node
  {
  private:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    BBox bbox = { glm::vec3(0.0), glm::vec3(0.0) };
    glm::mat4 transform;

    std::vector<Node*> children;

    uint64_t uid;

  public:
    Node(glm::mat4 transform);
    Node(glm::mat4 transform, std::vector<Vertex> vertices, std::vector<uint32_t> indices);
    Node(glm::mat4 transform, std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::vector<Node*> children);

    void SetMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
    void SetChildren(std::vector<Node*> children);

    const std::vector<Vertex>& GetVertices() const;
    const std::vector<uint32_t>& GetIndices() const;
    const std::vector<Node*>& GetChildren() const;

    std::vector<Vertex>& GetVertices();
    std::vector<uint32_t>& GetIndices();
    std::vector<Node*>& GetChildren();

    inline bool HasMesh() const { return indices.size() > 0; }

    void ForEach(glm::mat4 transform, std::function<void(const Node& n, glm::mat4 transform)> f) const;
  };

  class Loader
  {
  public:
    static std::pair<std::vector<Node>, Node*> FromGltf(Renderer& r, std::string filePath);
  };

}