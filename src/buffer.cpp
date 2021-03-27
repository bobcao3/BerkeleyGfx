#include "buffer.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

BG::MemoryAllocator::MemoryAllocator(vk::PhysicalDevice pDevice, vk::Device device, vk::Instance instance)
{
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
  allocatorInfo.physicalDevice = pDevice;
  allocatorInfo.device = device;
  allocatorInfo.instance = instance;

  vmaCreateAllocator(&allocatorInfo, &allocator);
}

BG::MemoryAllocator::~MemoryAllocator()
{
  vmaDestroyAllocator(allocator);
}

std::shared_ptr<BG::Buffer> BG::MemoryAllocator::Alloc(size_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = size;
  bufferInfo.usage = VkBufferUsageFlags(usage);

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = memoryUsage;

  VkBuffer buffer;
  VmaAllocation allocation;
  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

  return std::make_shared<BG::Buffer>(allocator, buffer, allocation);
}

BG::Buffer::Buffer(VmaAllocator& allocator, vk::Buffer buffer, VmaAllocation allocation)
  : allocator(allocator), buffer(buffer), allocation(allocation)
{
}

BG::Buffer::~Buffer()
{
  vmaDestroyBuffer(allocator, buffer, allocation);
}
