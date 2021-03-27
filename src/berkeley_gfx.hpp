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
  class Renderer;
  class Pipeline;
  class CommandBuffer;
  class MemoryAllocator;
  class Buffer;

  struct VertexBufferBinding {
    int binding;
  };
}