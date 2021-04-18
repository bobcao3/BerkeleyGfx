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
  class MeshSystem;
  class Pipeline;
  class Renderer;
  class TextureSystem;
  class Tracker;
  class BBox;

  struct VertexBufferBinding {
    int binding;
  };
}