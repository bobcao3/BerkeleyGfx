#pragma once

#include "berkeley_gfx.hpp"

#include <Vulkan/Vulkan.hpp>

namespace BG
{

  class CommandBuffer
  {
  private:
    vk::CommandBuffer m_buf;
    vk::Device m_device;

  public:
    void Begin();
    void End();

    void BeginRenderPass(
      Pipeline& p,
      vk::Framebuffer& frameBuffer,
      glm::uvec2 extent,
      glm::vec4 clearColor = glm::vec4(1.0),
      glm::ivec2 offset = glm::ivec2(0));
    void BindPipeline(Pipeline& p);
    void EndRenderPass();
    void Draw(uint32_t vertexCount, uint32_t firstVertex = 0, uint32_t instanceCount = 1, uint32_t firstInstance = 0);
    void BindVertexBuffer(VertexBufferBinding binding, std::shared_ptr<BG::Buffer> buffer, size_t offset);

    void WithRenderPass(
      Pipeline& p,
      vk::Framebuffer& frameBuffer,
      glm::uvec2 extent,
      glm::vec4 clearColor,
      glm::ivec2 offset,
      std::function<void()> func);

    void WithRenderPass(
      Pipeline& p,
      vk::Framebuffer& frameBuffer,
      glm::uvec2 extent,
      std::function<void()> func);

    CommandBuffer(vk::Device device, vk::CommandBuffer buf);
  };

}