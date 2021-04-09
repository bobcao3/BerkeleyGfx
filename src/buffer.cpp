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
