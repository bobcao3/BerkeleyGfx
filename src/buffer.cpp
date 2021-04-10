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

  for (uint32_t i = 0; i < maxFramesInFlight; i++)
  {
    m_buffers.push_back(AllocCPU2GPU(TRANSIENT_BLOCK_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc));
  }

  m_buffersBytesAllocated.resize(maxFramesInFlight);
}

BG::MemoryAllocator::~MemoryAllocator()
{
  vmaDestroyAllocator(allocator);
}

void BG::MemoryAllocator::NewFrame()
{
  m_currentFrame = (m_currentFrame + 1) % m_buffers.size();
  m_buffersBytesAllocated[m_currentFrame] = 0;
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

std::shared_ptr<BG::Image> BG::MemoryAllocator::AllocImage2D(glm::uvec2 extent, int mipLevels, vk::Format format, vk::ImageUsageFlags usage, vk::ImageLayout layout, VmaMemoryUsage memoryUsage)
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

  return std::make_shared<BG::Image>(allocator, image, allocation);
}

BG::MemoryAllocator::TransientAllocation BG::MemoryAllocator::AllocTransientUniformBuffer(size_t size)
{
  uint32_t currentAllocated = m_buffersBytesAllocated[m_currentFrame];
  m_buffersBytesAllocated[m_currentFrame] += uint32_t(size);
  return TransientAllocation{ m_buffers[m_currentFrame], currentAllocated };
}

BG::Image::Image(VmaAllocator& allocator, vk::Image image)
  : allocator(allocator), image(image)
{
}

BG::Image::Image(VmaAllocator& allocator, vk::Image image, VmaAllocation allocation, bool color, bool depth)
  : allocator(allocator), image(image), allocation(allocation), colorPlane(color), depthPlane(depth)
{
}

BG::Image::~Image()
{
  vmaDestroyImage(allocator, image, allocation);
}
