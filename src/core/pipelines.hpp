#pragma once

#include "berkeley_gfx.hpp"

#include <vulkan/vulkan.hpp>

namespace BG
{
  static bool glslangInitialized = false;

  class Pipeline
  {
  private:
    vk::UniqueShaderModule AddShaders(std::string shaders, int shaderType);

    vk::Device m_device;

    Renderer& r;
    
    vk::Viewport m_viewport;
    vk::Rect2D   m_scissor;

    vk::PipelineVertexInputStateCreateInfo         m_vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo       m_inputAssemblyInfo;
    vk::PipelineViewportStateCreateInfo            m_viewportInfo;
    vk::PipelineRasterizationStateCreateInfo       m_rasterizer;
    vk::PipelineMultisampleStateCreateInfo         m_multisampling;

    std::vector<vk::UniqueShaderModule>            m_shaderModules;
    std::vector<vk::PipelineShaderStageCreateInfo> m_stageCreateInfos;
    std::vector<vk::AttachmentDescription>         m_attachments;

    vk::AttachmentDescription m_depthAttachment;
    bool m_useDepthAttachment = false;

    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    vk::UniquePipelineLayout      m_layout;
    vk::UniqueRenderPass          m_renderpass;
    vk::UniquePipeline            m_pipeline;
    
    bool m_created = false;

    std::vector<vk::VertexInputBindingDescription> m_bindingDescriptions;
    std::vector<vk::VertexInputAttributeDescription> m_attributeDescriptions;
    std::vector<vk::DescriptorSetLayoutBinding> m_descSetLayoutBindings;
    std::vector<vk::DescriptorBindingFlags> m_descSetLayoutBindingFlags;
    std::vector<vk::PushConstantRange> m_pushConstants;

    std::vector<uint32_t> BuildProgramFromSrc(std::string shaders, int shaderType);
    
    std::unordered_map<std::string, uint32_t> m_name2bindings;
    std::unordered_map<std::string, uint32_t> m_memberOffsets;
    std::unordered_map<std::string, uint32_t> m_uniformBlockSize;

  public:
    void AddFragmentShaders(std::string shaders);
    void AddVertexShaders(std::string shaders);

    template <class T> VertexBufferBinding AddVertexBuffer(bool perVertex = true)
    {
      vk::VertexInputBindingDescription desc;
      desc.setStride(sizeof(T));
      desc.setInputRate(perVertex ? vk::VertexInputRate::eVertex : vk::VertexInputRate::eInstance);
      int binding = int(m_bindingDescriptions.size());
      desc.setBinding(binding);
      m_bindingDescriptions.push_back(desc);
      return VertexBufferBinding{ binding };
    }

    void AddAttribute(VertexBufferBinding binding, int location, vk::Format format, size_t offset);

    int GetBindingByName(std::string name);
    uint32_t GetMemberOffset(std::string name);

    void AddDescriptorUniform(int binding, vk::ShaderStageFlags stage, int count = 1, bool unbound = false);
    void AddDescriptorTexture(int binding, vk::ShaderStageFlags stage, int count = 1, bool unbound = false);

    void AddPushConstant(uint32_t offset, uint32_t size, vk::ShaderStageFlags stage);

    void SetViewport(float width, float height, float x = 0.0, float y = 0.0, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetScissor(int x, int y, int width, int height);

    void AddAttachment(vk::Format format, vk::ImageLayout initialLayout, vk::ImageLayout finalLayout, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
    void AddDepthAttachment(vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined, vk::ImageLayout finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal);

    void BuildPipeline();

    vk::DescriptorSet AllocDescSet(vk::DescriptorPool pool, int variableDescriptorCount = 0);

    void BindGraphicsUniformBuffer(Pipeline& p, vk::DescriptorSet descSet, const BG::Buffer& buffer, uint32_t offset, uint32_t range, int binding, int arrayElement = 0);
    void BindGraphicsImageView(Pipeline& p, vk::DescriptorSet descSet, vk::ImageView view, vk::ImageLayout layout, vk::Sampler sampler, int binding, int arrayElement = 0);

    vk::RenderPass GetRenderPass();
    vk::Pipeline GetPipeline();
    vk::PipelineLayout GetLayout();

    void BindRenderPass(
      vk::CommandBuffer& buf,
      vk::Framebuffer& frameBuffer,
      glm::uvec2 extent,
      glm::vec4 clearColor = glm::vec4(1.0),
      glm::ivec2 offset = glm::ivec2(0));

    Pipeline(Renderer& r, vk::Device device);

    static void InitBackend();
  };

}