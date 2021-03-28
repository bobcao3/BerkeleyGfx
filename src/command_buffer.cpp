#include "command_buffer.hpp"
#include "buffer.hpp"
#include "pipelines.hpp"

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

void BG::CommandBuffer::BindVertexBuffer(VertexBufferBinding binding, std::shared_ptr<BG::Buffer> buffer, size_t offset)
{
  vk::Buffer vertexBuffers[] = { buffer->buffer };
  vk::DeviceSize offsets[] = { offset };
  m_buf.bindVertexBuffers(binding.binding, 1, vertexBuffers, offsets);
}

void BG::CommandBuffer::BindIndexBuffer(std::shared_ptr<BG::Buffer> buffer, size_t offset, vk::IndexType indexType)
{
  m_buf.bindIndexBuffer(buffer->buffer, offset, indexType);
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

BG::CommandBuffer::CommandBuffer(vk::Device device, vk::CommandBuffer buf)
  : m_device(device), m_buf(buf)
{
}
