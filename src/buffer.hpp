#pragma once

#include "berkeley_gfx.hpp"

#include <vk_mem_alloc.h>
#include <Vulkan/Vulkan.hpp>

namespace BG
{

  class MemoryAllocator
  {
  private:
    VmaAllocator allocator;

  public:
    MemoryAllocator(vk::PhysicalDevice pDevice, vk::Device device, vk::Instance instance);
    ~MemoryAllocator();

    std::shared_ptr<Buffer> Alloc(size_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    inline std::shared_ptr<Buffer> Alloc(size_t size, vk::BufferUsageFlags usage) { return Alloc(size, usage, VMA_MEMORY_USAGE_GPU_ONLY); }
    inline std::shared_ptr<Buffer> AllocCPU2GPU(size_t size, vk::BufferUsageFlags usage) { return Alloc(size, usage, VMA_MEMORY_USAGE_CPU_TO_GPU); }
    inline std::shared_ptr<Buffer> AllocGPU2CPU(size_t size, vk::BufferUsageFlags usage) { return Alloc(size, usage, VMA_MEMORY_USAGE_GPU_TO_CPU); }
  };

  class Buffer
  {
  private:
    VmaAllocator& allocator;

  public:
    vk::Buffer buffer;
    VmaAllocation allocation;

    Buffer(VmaAllocator& allocator, vk::Buffer buffer, VmaAllocation allocation);
    ~Buffer();

    template <class T> T* Map() { void* pData; vmaMapMemory(allocator, allocation, &pData); return (T*)(pData); }
    inline void UnMap() { vmaUnmapMemory(allocator, allocation); };
  };
}