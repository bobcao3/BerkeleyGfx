#include "buffer.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

BG::MemoryAllocator::MemoryAllocator(vk::PhysicalDevice pDevice, vk::Device device, vk::Instance instance, uint32_t maxFramesInFlight)
{
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
  allocatorInfo.physicalDevice = pDevice;
  allocatorInfo.device = device;
  allocatorInfo.instance = instance;

  vmaCreateAllocator(&allocatorInfo, &allocator);

  m_buffers.resize(maxFramesInFlight);

  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = 0x100;
  bufferInfo.usage = VkBufferUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc);

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  uint32_t memoryTypeIndex;
  vmaFindMemoryTypeIndexForBufferInfo(allocator, &bufferInfo, &allocInfo, &memoryTypeIndex);

  // Create a pool that can have at most maxFramesInFlight * 4 blocks, 32 MiB each.
  VmaPoolCreateInfo poolCreateInfo = {};
  poolCreateInfo.memoryTypeIndex = memoryTypeIndex;
  poolCreateInfo.blockSize = 32ull * 1024 * 1024;
  poolCreateInfo.maxBlockCount = 4 * maxFramesInFlight;
  poolCreateInfo.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;

  vmaCreatePool(allocator, &poolCreateInfo, &transientPool);
}

BG::MemoryAllocator::~MemoryAllocator()
{
  for (auto& list : m_buffers)
  {
    list.clear();
  }
  vmaDestroyPool(allocator, transientPool);
  vmaDestroyAllocator(allocator);
}

void BG::MemoryAllocator::NewFrame()
{
  m_currentFrame = (m_currentFrame + 1) % m_buffers.size();
  m_buffers[m_currentFrame].clear();
}

std::unique_ptr<BG::Buffer> BG::MemoryAllocator::Alloc(size_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = size;
  bufferInfo.usage = VkBufferUsageFlags(usage);

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = memoryUsage;

  VkBuffer buffer;
  VmaAllocation allocation;
  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

  return std::make_unique<BG::Buffer>(allocator, buffer, allocation);
}

BG::Buffer::Buffer(VmaAllocator& allocator, vk::Buffer buffer, VmaAllocation allocation)
  : allocator(allocator), buffer(buffer), allocation(allocation)
{
}

BG::Buffer::~Buffer()
{
  vmaDestroyBuffer(allocator, buffer, allocation);
}

std::unique_ptr<BG::Image> BG::MemoryAllocator::AllocImage2D(glm::uvec2 extent, int mipLevels, vk::Format format, vk::ImageUsageFlags usage, vk::ImageLayout layout, VmaMemoryUsage memoryUsage)
{
  vk::ImageCreateInfo imageInfo;
  imageInfo.extent.width = extent.x;
  imageInfo.extent.height = extent.y;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.initialLayout = layout;
  imageInfo.tiling = vk::ImageTiling::eOptimal;
  imageInfo.usage = usage;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;
  imageInfo.samples = vk::SampleCountFlagBits::e1;

  VkImageCreateInfo _imageInfo = imageInfo;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = memoryUsage;

  VkImage image;
  VmaAllocation allocation;
  vmaCreateImage(allocator, &_imageInfo, &allocInfo, &image, &allocation, nullptr);

  return std::make_unique<BG::Image>(allocator, image, allocation);
}

BG::Buffer* BG::MemoryAllocator::AllocTransient(size_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = size;
  bufferInfo.usage = VkBufferUsageFlags(usage);

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = memoryUsage;
  allocInfo.pool = transientPool;

  VkBuffer buffer;
  VmaAllocation allocation;
  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

  auto ptr = std::make_unique<BG::Buffer>(allocator, buffer, allocation);

  Buffer* retVal = ptr.get();

  m_buffers[m_currentFrame].push_back(std::move(ptr));

  return retVal;
}

BG::Image::Image(VmaAllocator& allocator, vk::Image image)
  : allocator(allocator), image(image)
{
  allocated = false;
}

BG::Image::Image(VmaAllocator& allocator, vk::Image image, VmaAllocation allocation, bool color, bool depth)
  : allocator(allocator), image(image), allocation(allocation), colorPlane(color), depthPlane(depth)
{
}

BG::Image::~Image()
{
  if (allocated)
  {
    vmaDestroyImage(allocator, image, allocation);
  }
}
