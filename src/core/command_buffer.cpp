#include "command_buffer.hpp"
#include "buffer.hpp"
#include "pipelines.hpp"
#include "lifetime_tracker.hpp"

void BG::CommandBuffer::Begin()
{
  m_buf.begin(vk::CommandBufferBeginInfo{ {}, nullptr });
}

void BG::CommandBuffer::End()
{
  m_buf.end();
}

void BG::CommandBuffer::BeginRenderPass(Pipeline& p, vk::Framebuffer& frameBuffer, glm::uvec2 extent, glm::vec4 clearColor, glm::ivec2 offset)
{
  p.BindRenderPass(m_buf, frameBuffer, extent, clearColor, offset);
}

void BG::CommandBuffer::BindPipeline(Pipeline& p)
{
  m_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, p.GetPipeline());
}

void BG::CommandBuffer::EndRenderPass()
{
  m_buf.endRenderPass();
}

void BG::CommandBuffer::Draw(uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
  m_buf.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void BG::CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t instanceCount, uint32_t firstInstance)
{
  m_buf.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void BG::CommandBuffer::BindVertexBuffer(VertexBufferBinding binding, const BG::Buffer& buffer, size_t offset)
{
  vk::Buffer vertexBuffers[] = { buffer.buffer };
  vk::DeviceSize offsets[] = { offset };
  m_buf.bindVertexBuffers(binding.binding, 1, vertexBuffers, offsets);
}

void BG::CommandBuffer::BindIndexBuffer(const BG::Buffer& buffer, size_t offset, vk::IndexType indexType)
{
  m_buf.bindIndexBuffer(buffer.buffer, offset, indexType);
}

void BG::CommandBuffer::PushConstants(Pipeline& p, vk::ShaderStageFlagBits stage, uint32_t offset, uint32_t size, const void* data)
{
  m_buf.pushConstants(p.GetLayout(), stage, offset, size, data);
}

void BG::CommandBuffer::BindGraphicsDescSets(Pipeline& p, vk::DescriptorSet descSet, int set)
{
  m_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, p.GetLayout(), set, 1, &descSet, 0, nullptr);
}

vk::AccessFlags getAccessFlags(vk::ImageLayout layout, bool read)
{
  switch (layout)
  {
  case vk::ImageLayout::eUndefined:
    return vk::AccessFlags(0);
  case vk::ImageLayout::eGeneral:
    return vk::AccessFlags(0);
  case vk::ImageLayout::eColorAttachmentOptimal:
    return read ? vk::AccessFlagBits::eColorAttachmentRead : vk::AccessFlagBits::eColorAttachmentWrite;
  case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    return read ? vk::AccessFlagBits::eDepthStencilAttachmentRead : vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
    return vk::AccessFlagBits::eDepthStencilAttachmentRead;
  case vk::ImageLayout::eShaderReadOnlyOptimal:
    return read ? vk::AccessFlagBits::eMemoryRead : vk::AccessFlagBits::eMemoryWrite;
  case vk::ImageLayout::eTransferSrcOptimal:
    return vk::AccessFlagBits::eTransferRead;
  case vk::ImageLayout::eTransferDstOptimal:
    return vk::AccessFlagBits::eTransferWrite;
  case vk::ImageLayout::ePreinitialized:
    return vk::AccessFlags(0);
  case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal:
    return vk::AccessFlagBits::eDepthStencilAttachmentRead;
  case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal:
    return vk::AccessFlagBits::eDepthStencilAttachmentRead;
  case vk::ImageLayout::eDepthAttachmentOptimal:
    return read ? vk::AccessFlagBits::eDepthStencilAttachmentRead : vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  case vk::ImageLayout::eDepthReadOnlyOptimal:
    return vk::AccessFlagBits::eDepthStencilAttachmentRead;
  case vk::ImageLayout::eStencilAttachmentOptimal:
    return read ? vk::AccessFlagBits::eDepthStencilAttachmentRead : vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  case vk::ImageLayout::eStencilReadOnlyOptimal:
    return vk::AccessFlagBits::eDepthStencilAttachmentRead;
  case vk::ImageLayout::ePresentSrcKHR:
    return vk::AccessFlags(0);
  case vk::ImageLayout::eSharedPresentKHR:
    return vk::AccessFlags(0);
  case vk::ImageLayout::eShadingRateOptimalNV:
    return vk::AccessFlags(0);
  default:
    return vk::AccessFlags(0);
  }
}

void BG::CommandBuffer::ImageTransition(const BG::Image& image, vk::PipelineStageFlags fromStage, vk::PipelineStageFlags toStage, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, int baseMip, int levels, int baseLayer, int layers)
{
  vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits(0);
  if (image.HasColorPlane()) aspect |= vk::ImageAspectFlagBits::eColor;
  if (image.HasDepthPlane()) aspect |= vk::ImageAspectFlagBits::eDepth;

  ImageTransition(image.image, fromStage, toStage, oldLayout, newLayout, aspect, baseMip, levels, baseLayer, layers);
}

void BG::CommandBuffer::ImageTransition(vk::Image image, vk::PipelineStageFlags fromStage, vk::PipelineStageFlags toStage, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect, int baseMip, int levels, int baseLayer, int layers)
{

  vk::ImageMemoryBarrier barrierToTransfer;
  barrierToTransfer.oldLayout = oldLayout;
  barrierToTransfer.newLayout = newLayout;
  barrierToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrierToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrierToTransfer.image = image;
  barrierToTransfer.subresourceRange.aspectMask = aspect;
  barrierToTransfer.subresourceRange.baseMipLevel = baseMip;
  barrierToTransfer.subresourceRange.levelCount = levels;
  barrierToTransfer.subresourceRange.baseArrayLayer = baseLayer;
  barrierToTransfer.subresourceRange.layerCount = layers;
  barrierToTransfer.srcAccessMask = getAccessFlags(oldLayout, false);
  barrierToTransfer.dstAccessMask = getAccessFlags(newLayout, true);

  m_buf.pipelineBarrier(fromStage, toStage, vk::DependencyFlags(0), 0, nullptr, 0, nullptr, 1, &barrierToTransfer);
}

void BG::CommandBuffer::WithRenderPass(Pipeline& p, vk::Framebuffer& frameBuffer, glm::uvec2 extent, glm::vec4 clearColor, glm::ivec2 offset, std::function<void()> func)
{
  this->BeginRenderPass(p, frameBuffer, extent, clearColor, offset);
  func();
  this->EndRenderPass();
}

void BG::CommandBuffer::WithRenderPass(Pipeline& p, vk::Framebuffer& frameBuffer, glm::uvec2 extent, std::function<void()> func)
{
  WithRenderPass(p, frameBuffer, extent, glm::vec4(0.0), glm::ivec2(0), func);
}

void BG::CommandBuffer::WithRenderPass(Pipeline& p, std::vector<vk::ImageView> renderTargets, glm::uvec2 extent, glm::vec4 clearColor, glm::ivec2 offset, std::function<void()> func)
{
  vk::FramebufferCreateInfo framebufferInfo;
  framebufferInfo.setRenderPass(p.GetRenderPass());
  framebufferInfo.setAttachments(renderTargets);
  framebufferInfo.setWidth(extent.x);
  framebufferInfo.setHeight(extent.y);
  framebufferInfo.setLayers(1);

  auto fb = m_device.createFramebufferUnique(framebufferInfo);

  WithRenderPass(p, fb.get(), extent, glm::vec4(0.0), glm::ivec2(0), func);

  m_tracker.DisposeFramebuffer(std::move(fb));
}

void BG::CommandBuffer::WithRenderPass(Pipeline& p, std::vector<vk::ImageView> renderTargets, glm::uvec2 extent, std::function<void()> func)
{
  WithRenderPass(p, renderTargets, extent, glm::vec4(0.0), glm::ivec2(0), func);
}

BG::CommandBuffer::CommandBuffer(vk::Device device, vk::CommandBuffer buf, BG::Tracker& tracker)
  : m_device(device), m_buf(buf), m_tracker(tracker)
{
}
