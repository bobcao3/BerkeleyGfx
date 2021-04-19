#include "mesh_system.hpp"
#include "renderer.hpp"
#include "texture_system.hpp"

// Import the tinyGlTF library to load glTF models
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

using namespace BG;
using namespace BG::MeshSystem;

Node::Node(glm::mat4 transform)
  : transform(transform), uid(GetUID())
{
}

Node::Node(glm::mat4 transform, std::vector<Vertex> vertices, std::vector<uint32_t> indices)
  : Node(transform)
{
  this->vertices = vertices;
  this->indices = indices;
}

Node::Node(glm::mat4 transform, std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::vector<Node*> children)
  : Node(transform, vertices, indices)
{
  this->children = children;
}

void Node::SetMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
{
  this->vertices = vertices;
  this->indices = indices;

  uid = GetUID();
}

void Node::SetChildren(std::vector<Node*> children)
{
  this->children = children;

  uid = GetUID();
}

const std::vector<Vertex>& Node::GetVertices() const
{
  return vertices;
}

const std::vector<uint32_t>& Node::GetIndices() const
{
  return indices;
}

const std::vector<Node*>& Node::GetChildren() const
{
  return children;
}

std::vector<Vertex>& Node::GetVertices()
{
  return vertices;
}

std::vector<uint32_t>& Node::GetIndices()
{
  return indices;
}

std::vector<Node*>& Node::GetChildren()
{
  return children;
}

void BG::MeshSystem::Node::ForEach(glm::mat4 transform, std::function<void(const Node& n, glm::mat4 transform)> f) const
{
  glm::mat4 absoluteTransform = this->transform * transform;

  f(*this, absoluteTransform);

  for (auto child : children) child->ForEach(absoluteTransform, f);
}

void load_gltf_node(tinygltf::Model& model, std::vector<Node>& nodes, int nodeId)
{
  auto& nodeGltf = model.nodes[nodeId];
  auto& node = nodes[nodeId];

  // Recursively go through all child nodes
  for (int childNodeId : nodeGltf.children)
  {
    node.GetChildren().push_back(&nodes[childNodeId]);
    load_gltf_node(model, nodes, childNodeId);
  }
}

std::pair<std::vector<Node>, Node*> BG::MeshSystem::Loader::FromGltf(Renderer& r, std::string filePath)
{
  std::vector<Node> nodes;

  tinygltf::Model model;
  
  {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // Here we are loadinga *.gltf file which is an ASCII json.
    // If we want to load *.glb (binary package), we should use the LoadBinaryFromFile function
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);

    // Check whether the library successfully loaded the glTF model
    if (!warn.empty()) {
      spdlog::warn("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
      spdlog::error("Err: %s\n", err.c_str());
      throw std::runtime_error("glTF parsing error");
    }

    if (!ret) {
      spdlog::error("Failed to parse glTF");
      throw std::runtime_error("Fail to parse glTF");
    }
  }

  for (auto& nodeGltf : model.nodes)
  {
    glm::mat4 localTransform = glm::mat4(1.0);
    if (nodeGltf.matrix.size() == 16)
    {
      std::copy(nodeGltf.matrix.begin(), nodeGltf.matrix.end(), &localTransform[0].x);
      localTransform = glm::transpose(localTransform);
    }

    auto& node = nodes.emplace_back(localTransform);

    // The node contains a mesh
    if (nodeGltf.mesh >= 0)
    {
      auto& mesh = model.meshes[nodeGltf.mesh];

      spdlog::info("======== NODE {} ========", nodeGltf.name);

      uint32_t vertexOffset = 0;

      // Iterate through all primitives of the mesh
      for (auto& primitive : mesh.primitives)
      {
        auto& material = model.materials[primitive.material];

        // Get the vertex position accessor (and relavent buffers)
        auto& positionAccessor = model.accessors[primitive.attributes["POSITION"]];
        spdlog::info("Position {}x{}, offset = {}", positionAccessor.count, positionAccessor.ByteStride(model.bufferViews[positionAccessor.bufferView]), positionAccessor.byteOffset);

        auto& positionBufferView = model.bufferViews[positionAccessor.bufferView];
        auto& positionBuffer = model.buffers[positionBufferView.buffer];

        size_t positionBufferStride = positionAccessor.ByteStride(model.bufferViews[positionAccessor.bufferView]);

        // Get the index accessor (and relavent buffers)
        auto& indexAccessor = model.accessors[primitive.indices];
        spdlog::info("Index {}x{}, offset = {}", indexAccessor.count, indexAccessor.ByteStride(model.bufferViews[indexAccessor.bufferView]), indexAccessor.byteOffset);

        auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        auto& indexBuffer = model.buffers[indexBufferView.buffer];

        size_t indexBufferStride = indexAccessor.ByteStride(model.bufferViews[indexAccessor.bufferView]);

        // Get the texture UV accessor (and relavent buffers)
        int texcoordIndex = material.pbrMetallicRoughness.baseColorTexture.texCoord;
        int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
        std::stringstream texcoordNameBuilder;
        texcoordNameBuilder << "TEXCOORD_" << texcoordIndex;

        auto& uvAccessor = model.accessors[primitive.attributes[texcoordNameBuilder.str()]];
        spdlog::info("{} {}x{}, offset = {}", texcoordNameBuilder.str(), uvAccessor.count, uvAccessor.ByteStride(model.bufferViews[uvAccessor.bufferView]), uvAccessor.byteOffset);

        auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
        auto& uvBuffer = model.buffers[uvBufferView.buffer];

        size_t uvBufferStride = uvAccessor.ByteStride(model.bufferViews[uvAccessor.bufferView]);

        // Push all vertices
        for (size_t index = 0; index < positionAccessor.count; index++)
        {
          // Get the base address and store the vertex
          float* elementBase = (float*)((uint8_t*)(positionBuffer.data.data()) + positionBufferView.byteOffset + positionAccessor.byteOffset + positionBufferStride * index);
          float* uvElementBase = (float*)((uint8_t*)(uvBuffer.data.data()) + uvBufferView.byteOffset + uvAccessor.byteOffset + uvBufferStride * index);
          Vertex v;
          v.pos = glm::vec3(elementBase[0], elementBase[1], elementBase[2]);
          v.uv0 = glm::vec2(uvElementBase[0], uvElementBase[1]);
          v.materialIndex = textureIndex;
          node.GetVertices().push_back(v);
        }

        // Push all indices
        uint32_t indexStride = indexAccessor.ByteStride(model.bufferViews[indexAccessor.bufferView]);
        if (indexStride == 2)
        {
          for (size_t index = 0; index < indexAccessor.count; index++)
          {
            uint16_t* elementBase = (uint16_t*)((uint8_t*)(indexBuffer.data.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset + indexBufferStride * index);
            node.GetIndices().push_back(uint32_t(elementBase[0]) + vertexOffset);
          }
        }
        else if (indexStride == 4)
        {
          for (size_t index = 0; index < indexAccessor.count; index++)
          {
            uint32_t* elementBase = (uint32_t*)((uint8_t*)(indexBuffer.data.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset + indexBufferStride * index);
            node.GetIndices().push_back(uint32_t(elementBase[0]) + vertexOffset);
          }
        }

        vertexOffset += positionAccessor.count;
      }
    }
  }

  Node& rootNode = nodes.emplace_back(glm::mat4(1.0));
  
  for (auto nodeId : model.scenes[model.defaultScene].nodes)
  {
    load_gltf_node(model, nodes, nodeId);
    rootNode.GetChildren().push_back(&nodes[nodeId]);
  }

  // Load the images
  for (auto img : model.images)
  {
    r.getTextureSystem().AddTexture(img.image.data(), img.width, img.height, img.image.size(), vk::Format::eR8G8B8A8Srgb);
  }

  return std::pair<std::vector<Node>, Node*>(std::move(nodes), &rootNode);
}
