#include "texture_system.hpp"

#include "buffer.hpp"
#include "renderer.hpp"
#include "command_buffer.hpp"

using namespace BG;

TextureSystem::Handle TextureSystem::AddTexture(uint8_t* imageBuffer, int width, int height, size_t size, vk::Format format)
{
  auto image = m_allocator.AllocImage2D(glm::uvec2(width, height), 1, format, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::ImageLayout::eUndefined);

  int index = int(m_images.size());

  vk::ImageViewCreateInfo viewInfo;
  viewInfo.image = image->image;
  viewInfo.viewType = vk::ImageViewType::e2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  auto stagingBuffer = m_allocator.AllocCPU2GPU(size, vk::BufferUsageFlagBits::eTransferSrc);
  uint8_t* stagingBufferGPU = stagingBuffer->Map<uint8_t>();
  std::copy(imageBuffer, imageBuffer + size, stagingBufferGPU);
  stagingBuffer->UnMap();

  vk::BufferImageCopy copy;
  copy.bufferOffset = 0;
  copy.bufferRowLength = width;
  copy.bufferImageHeight = height;
  copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  copy.imageSubresource.mipLevel = 0;
  copy.imageSubresource.baseArrayLayer = 0;
  copy.imageSubresource.layerCount = 1;
  copy.imageExtent.width = width;
  copy.imageExtent.height = height;
  copy.imageExtent.depth = 1;
  copy.imageOffset.x = 0;
  copy.imageOffset.y = 0;
  copy.imageOffset.z = 0;
  
  auto _cmdBuf = m_renderer.AllocCmdBuffer();
  CommandBuffer cmdBuf(m_device, _cmdBuf.get(), m_renderer.getTracker());

  cmdBuf.Begin();
  cmdBuf.ImageTransition(*image, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  cmdBuf.GetVkCmdBuf().copyBufferToImage(stagingBuffer->buffer, image->image, vk::ImageLayout::eTransferDstOptimal, 1, &copy);
  cmdBuf.ImageTransition(*image, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
  cmdBuf.End();

  m_images.push_back(std::move(image));
  m_imageViews.push_back(m_device.createImageViewUnique(viewInfo));

  m_renderer.SubmitCmdBufferNow(cmdBuf.GetVkCmdBuf());

  return Handle{ index };
}

TextureSystem::TextureSystem(vk::Device device, MemoryAllocator& allocator, Renderer& renderer)
  : m_device(device), m_allocator(allocator), m_renderer(renderer)
{
  vk::SamplerCreateInfo samplerInfo;
  samplerInfo.magFilter = vk::Filter::eLinear;
  samplerInfo.minFilter = vk::Filter::eLinear;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.compareEnable = false;
  samplerInfo.compareOp = vk::CompareOp::eAlways;
  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
  samplerInfo.mipLodBias = 0.0;
  samplerInfo.minLod = 0.0;
  samplerInfo.maxLod = 0.0;

  m_samplerBilinear = m_device.createSamplerUnique(samplerInfo);
}
