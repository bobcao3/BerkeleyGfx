#pragma once

#include "berkeley_gfx.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace BG
{

  class MemoryAllocator
  {
  private:
    VmaAllocator allocator;

    uint32_t m_currentFrame;

    std::vector<std::vector<std::unique_ptr<Buffer>>> m_buffers;

    VmaPool transientPool;

  public:

    MemoryAllocator(vk::PhysicalDevice pDevice, vk::Device device, vk::Instance instance, uint32_t maxFramesInFlight);
    ~MemoryAllocator();

    void NewFrame();

    // Static allocation
    std::unique_ptr<Buffer> Alloc(size_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    inline std::unique_ptr<Buffer> Alloc(size_t size, vk::BufferUsageFlags usage) { return Alloc(size, usage, VMA_MEMORY_USAGE_GPU_ONLY); }
    inline std::unique_ptr<Buffer> AllocCPU2GPU(size_t size, vk::BufferUsageFlags usage) { return Alloc(size, usage, VMA_MEMORY_USAGE_CPU_TO_GPU); }
    inline std::unique_ptr<Buffer> AllocGPU2CPU(size_t size, vk::BufferUsageFlags usage) { return Alloc(size, usage, VMA_MEMORY_USAGE_GPU_TO_CPU); }

    std::unique_ptr<Image> AllocImage2D(
      glm::uvec2 extent, int mipLevels, vk::Format format, vk::ImageUsageFlags usage,
      vk::ImageLayout layout = vk::ImageLayout::eUndefined, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer* AllocTransient(size_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU);
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

  class Image
  {
  private:
    VmaAllocator& allocator;

    bool colorPlane = true;
    bool depthPlane = false;

    bool allocated = true;

  public:
    vk::Image image;
    VmaAllocation allocation;

    Image(VmaAllocator& allocator, vk::Image image);
    Image(VmaAllocator& allocator, vk::Image image, VmaAllocation allocation, bool color = true, bool depth = false);
    ~Image();

    inline bool HasColorPlane() const { return colorPlane; }
    inline bool HasDepthPlane() const { return depthPlane; }

    template <class T> T* Map() { void* pData; vmaMapMemory(allocator, allocation, &pData); return (T*)(pData); }
    inline void UnMap() { vmaUnmapMemory(allocator, allocation); };

  };
}