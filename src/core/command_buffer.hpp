#pragma once

#include "berkeley_gfx.hpp"

#include <vulkan/vulkan.hpp>

namespace BG
{

  class CommandBuffer
  {
  private:
    vk::CommandBuffer m_buf;
    vk::Device m_device;
    Tracker& m_tracker;

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
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t vertexOffset = 0, uint32_t instanceCount = 1, uint32_t firstInstance = 0);
    void BindVertexBuffer(VertexBufferBinding binding, const BG::Buffer& buffer, size_t offset);
    void BindIndexBuffer(const BG::Buffer& buffer, size_t offset, vk::IndexType indexType = vk::IndexType::eUint32);
    
    void PushConstants(Pipeline& p, vk::ShaderStageFlagBits stage, uint32_t offset, uint32_t size, const void* data);
    template <class T> void PushConstants(Pipeline& p, vk::ShaderStageFlagBits stage, uint32_t offset, T& data) {
      PushConstants(p, stage, offset, sizeof(data), &data);
    }

    void BindGraphicsDescSets(Pipeline& p, vk::DescriptorSet descSet, int set = 0);

    void ImageTransition(
      const BG::Image& image,
      vk::PipelineStageFlags fromStage, vk::PipelineStageFlags toStage,
      vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
      int baseMip = 0, int levels = 1, int baseLayer = 0, int layers = 1);

    void ImageTransition(
      vk::Image image,
      vk::PipelineStageFlags fromStage, vk::PipelineStageFlags toStage,
      vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
      vk::ImageAspectFlags aspect,
      int baseMip = 0, int levels = 1, int baseLayer = 0, int layers = 1);

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

    void WithRenderPass(
      Pipeline& p,
      std::vector<vk::ImageView> renderTargets,
      glm::uvec2 extent,
      glm::vec4 clearColor,
      glm::ivec2 offset,
      std::function<void()> func);

    void WithRenderPass(
      Pipeline& p,
      std::vector<vk::ImageView> renderTargets,
      glm::uvec2 extent,
      std::function<void()> func);

    CommandBuffer(vk::Device device, vk::CommandBuffer buf, BG::Tracker& tracker);

    inline vk::CommandBuffer GetVkCmdBuf() { return m_buf; }
  };

}