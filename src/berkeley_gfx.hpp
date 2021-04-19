#pragma once

#define GLFW_INCLUDE_VULKAN

#include <memory>
#include <vector>
#include <string>
#include <iostream>

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

namespace BG
{
  class Buffer;
  class CommandBuffer;
  class Image;
  class MemoryAllocator;
  class Pipeline;
  class Renderer;
  class TextureSystem;
  class Tracker;
  class BBox;

  namespace MeshSystem
  {
    struct Vertex;
    class Node;
    class Loader;
  }

  struct VertexBufferBinding {
    int binding;
  };

  uint64_t GetUID();
}