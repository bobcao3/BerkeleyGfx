#pragma once

#include "berkeley_gfx.hpp"
#include "bbox.hpp"

#include <vulkan/vulkan.hpp>

namespace BG
{

  class MeshSystem
  {
  private:
    Renderer& r;



  public:

    struct Vertex
    {
      glm::vec3 pos;
      int materialIndex;
      glm::vec3 normal;
      glm::vec2 uv0;
      glm::vec2 uv1;
    };

    struct Node
    {
      // CPU Side data
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      BBox bbox;
      glm::mat4 transform;

      bool hasMesh;

      // GPU Side data
      uint32_t firstVertex;
      uint32_t firstIndex;
    };

    struct Scene
    {
      std::vector<Node> nodes;
      uint32_t rootNodeIndex = 0;
    };

  public:

  };


}