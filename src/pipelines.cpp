#include "pipelines.hpp"
#include "buffer.hpp"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

using namespace BG;

std::vector<uint32_t> BuildSPIRV(glslang::TProgram& program, EShLanguage shaderType)
{
  std::vector<unsigned int> spirv;
  spv::SpvBuildLogger logger;
  glslang::SpvOptions spvOptions;
  glslang::GlslangToSpv(*program.getIntermediate(shaderType), spirv, &logger, &spvOptions);

  return spirv;
}

std::vector<uint32_t> BuildProgramFromSrc(std::string shaders, EShLanguage shaderType)
{
  const char* shaderCStr = shaders.c_str();

  glslang::TShader shader(shaderType);
  shader.setStrings(&shaderCStr, 1);

  int ClientInputSemanticsVersion = 100;
  glslang::EShTargetClientVersion VulkanClientVersion = glslang::EShTargetVulkan_1_0;
  glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;

  shader.setEnvInput(glslang::EShSourceGlsl, shaderType, glslang::EShClientVulkan, ClientInputSemanticsVersion);
  shader.setEnvClient(glslang::EShClientVulkan, VulkanClientVersion);
  shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);

  TBuiltInResource Resources{};

  Resources.maxDrawBuffers = true;
  Resources.limits.generalVariableIndexing = true;

  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

  const int DefaultVersion = 100;

  if (!shader.parse(&Resources, 100, false, messages))
  {
    spdlog::error("GLSL Parsing Failed\n{}{}", shader.getInfoLog(), shader.getInfoDebugLog());
  }

  glslang::TProgram program;
  program.addShader(&shader);

  if (!program.link(messages))
  {
    spdlog::error("Link failed");
  }

  return BuildSPIRV(program, shaderType);
}

vk::UniqueShaderModule BG::Pipeline::AddShaders(std::string shaders, int _shaderType)
{
  EShLanguage shaderType = EShLanguage(_shaderType);

  std::vector<uint32_t> spirv = BuildProgramFromSrc(shaders, shaderType);

  auto shaderModule = m_device.createShaderModuleUnique({ {}, spirv });

  return shaderModule;
}

void BG::Pipeline::AddFragmentShaders(std::string shaders)
{
  auto shader = AddShaders(shaders, EShLangFragment);

  m_stageCreateInfos.push_back(vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, shader.get(), "main" });

  m_shaderModules.push_back(std::move(shader));
}

void BG::Pipeline::AddVertexShaders(std::string shaders)
{
  auto shader = AddShaders(shaders, EShLangVertex);

  m_stageCreateInfos.push_back(vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, shader.get(), "main" });

  m_shaderModules.push_back(std::move(shader));
}

void BG::Pipeline::AddAttribute(VertexBufferBinding binding, int location, vk::Format format, size_t offset)
{
  vk::VertexInputAttributeDescription desc;
  desc.setBinding(binding.binding);
  desc.setLocation(location);
  desc.setFormat(format);
  desc.setOffset(offset);

  m_attributeDescriptions.push_back(desc);
}

void BG::Pipeline::AddDescriptorUniform(int binding, vk::ShaderStageFlags stage, int count)
{
  vk::DescriptorSetLayoutBinding layoutBinding;
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stage;
  layoutBinding.pImmutableSamplers = nullptr;

  m_descSetLayoutBindings.push_back(layoutBinding);
}

void BG::Pipeline::AddDescriptorTexture(int binding, vk::ShaderStageFlags stage, int count)
{
  vk::DescriptorSetLayoutBinding layoutBinding;
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stage;
  layoutBinding.pImmutableSamplers = nullptr;

  m_descSetLayoutBindings.push_back(layoutBinding);
}

void BG::Pipeline::SetViewport(float width, float height, float x, float y, float minDepth, float maxDepth)
{
  m_viewport.x = x;
  m_viewport.y = y;
  m_viewport.width = width;
  m_viewport.height = height;
  m_viewport.minDepth = minDepth;
  m_viewport.maxDepth = maxDepth;

  SetScissor(int(x), int(y), int(width), int(height));

  m_viewportInfo.viewportCount = 1;
  m_viewportInfo.pViewports = &m_viewport;
  m_viewportInfo.scissorCount = 1;
  m_viewportInfo.pScissors = &m_scissor;
}

void BG::Pipeline::SetScissor(int x, int y, int width, int height)
{
  m_scissor.offset.x = x;
  m_scissor.offset.y = y;
  m_scissor.extent.width = width;
  m_scissor.extent.height = height;
}

void BG::Pipeline::AddAttachment(vk::Format format, vk::ImageLayout initialLayout, vk::ImageLayout finalLayout, vk::SampleCountFlagBits samples)
{
  vk::AttachmentDescription attachment;
  attachment.format = format;
  attachment.samples = samples;
  attachment.initialLayout = initialLayout;
  attachment.finalLayout = finalLayout;

  attachment.loadOp = vk::AttachmentLoadOp::eClear;
  attachment.storeOp = vk::AttachmentStoreOp::eStore;
  attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

  attachment.initialLayout = initialLayout;
  attachment.finalLayout = finalLayout;

  m_attachments.push_back(attachment);
}

void BG::Pipeline::AddDepthAttachment(vk::ImageLayout initialLayout, vk::ImageLayout finalLayout)
{
  m_depthAttachment.format = vk::Format::eD32Sfloat;
  m_depthAttachment.samples = vk::SampleCountFlagBits::e1;
  m_depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  m_depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
  m_depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  m_depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  m_depthAttachment.initialLayout = initialLayout;
  m_depthAttachment.finalLayout = finalLayout;

  m_useDepthAttachment = true;
}

void BG::Pipeline::BuildPipeline()
{
  vk::DescriptorSetLayoutCreateInfo layoutInfo;
  layoutInfo.setBindings(m_descSetLayoutBindings);

  m_descriptorSetLayout = m_device.createDescriptorSetLayoutUnique(layoutInfo);

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout.get();

  m_layout = m_device.createPipelineLayoutUnique(pipelineLayoutInfo);

  std::vector<vk::AttachmentReference> attachments;

  uint32_t i = 0;
  for (auto& attachment : m_attachments)
  {
    attachments.push_back({ i, vk::ImageLayout::eColorAttachmentOptimal });
    i++;
  }

  vk::AttachmentReference depthAttachmentRef;

  if (m_useDepthAttachment)
  {
    depthAttachmentRef.attachment = i;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
  }

  std::vector<vk::SubpassDescription> subpass;
  vk::SubpassDescription mainSubpass;
  mainSubpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
  mainSubpass.setColorAttachments(attachments);
  mainSubpass.setPDepthStencilAttachment(&depthAttachmentRef);
  subpass.push_back(mainSubpass);

  if (m_useDepthAttachment)
  {
    std::vector<vk::AttachmentDescription> allAttachements;
    allAttachements = m_attachments;
    allAttachements.push_back(m_depthAttachment);

    m_renderpass = m_device.createRenderPassUnique({ {}, allAttachements, subpass });
  }
  else
  {
    m_renderpass = m_device.createRenderPassUnique({ {}, m_attachments, subpass });
  }

  std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;

  for (auto& attachment : m_attachments)
  {
    colorBlendAttachments.push_back({
      false,
      vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
      vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
      });
  }

  vk::PipelineColorBlendStateCreateInfo blendInfo;
  blendInfo.setLogicOpEnable(false);
  blendInfo.setAttachments(colorBlendAttachments);

  m_vertexInputInfo.setVertexBindingDescriptions(m_bindingDescriptions);
  m_vertexInputInfo.setVertexAttributeDescriptions(m_attributeDescriptions);

  vk::PipelineDepthStencilStateCreateInfo depthStencilState;
  if (m_useDepthAttachment)
  {
    depthStencilState.depthTestEnable = true;
    depthStencilState.depthWriteEnable = true;
    depthStencilState.depthCompareOp = vk::CompareOp::eLess;
    depthStencilState.depthBoundsTestEnable = false;
  }

  vk::GraphicsPipelineCreateInfo pipelineInfo;
  pipelineInfo.setStages(m_stageCreateInfos);
  pipelineInfo.pVertexInputState = &m_vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &m_inputAssemblyInfo;
  pipelineInfo.pViewportState = &m_viewportInfo;
  pipelineInfo.pRasterizationState = &m_rasterizer;
  pipelineInfo.pMultisampleState = &m_multisampling;
  pipelineInfo.pDepthStencilState = m_useDepthAttachment ? &depthStencilState : nullptr;
  pipelineInfo.pColorBlendState = &blendInfo;
  pipelineInfo.pDynamicState = nullptr;
  pipelineInfo.layout = m_layout.get();
  pipelineInfo.renderPass = m_renderpass.get();
  pipelineInfo.subpass = 0;
  
  m_pipeline = m_device.createGraphicsPipelineUnique(nullptr, pipelineInfo, nullptr);

  m_created = true;
}

vk::DescriptorSet Pipeline::AllocDescSet(vk::DescriptorPool pool)
{
  vk::DescriptorSetAllocateInfo allocInfo;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_descriptorSetLayout.get();

  return m_device.allocateDescriptorSets(allocInfo)[0];
}


void BG::Pipeline::BindGraphicsUniformBuffer(Pipeline& p, vk::DescriptorSet descSet, std::shared_ptr<BG::Buffer> buffer, uint32_t offset, uint32_t range, int binding, int arrayElement)
{
  vk::DescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = buffer->buffer;
  bufferInfo.offset = offset;
  bufferInfo.range = range;

  vk::WriteDescriptorSet descSetWrite;
  descSetWrite.dstBinding = binding;
  descSetWrite.dstArrayElement = arrayElement;
  descSetWrite.dstSet = descSet;
  descSetWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
  descSetWrite.descriptorCount = 1;
  descSetWrite.pBufferInfo = &bufferInfo;

  m_device.updateDescriptorSets(1, &descSetWrite, 0, nullptr);
}

void BG::Pipeline::BindGraphicsImageView(Pipeline& p, vk::DescriptorSet descSet, vk::ImageView view, vk::ImageLayout layout, vk::Sampler sampler, int binding, int arrayElement)
{
  vk::DescriptorImageInfo imageInfo;
  imageInfo.imageLayout = layout;
  imageInfo.imageView = view;
  imageInfo.sampler = sampler;

  vk::WriteDescriptorSet descSetWrite;
  descSetWrite.dstBinding = binding;
  descSetWrite.dstArrayElement = arrayElement;
  descSetWrite.dstSet = descSet;
  descSetWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descSetWrite.descriptorCount = 1;
  descSetWrite.pImageInfo = &imageInfo;

  m_device.updateDescriptorSets(1, &descSetWrite, 0, nullptr);
}

vk::RenderPass Pipeline::GetRenderPass()
{
  if (m_created)
  {
    return m_renderpass.get();
  }
  else
  {
    spdlog::error("Pipeline is not built");
    throw std::runtime_error("Pipeline is not built");
  }
}

vk::Pipeline Pipeline::GetPipeline()
{
  if (m_created)
  {
    return m_pipeline.get();
  }
  else
  {
    spdlog::error("Pipeline is not built");
    throw std::runtime_error("Pipeline is not built");
  }
}

vk::PipelineLayout Pipeline::GetLayout()
{
  if (m_created)
  {
    return m_layout.get();
  }
  else
  {
    spdlog::error("Pipeline is not built");
    throw std::runtime_error("Pipeline is not built");
  }
}

void BG::Pipeline::BindRenderPass(
  vk::CommandBuffer& buf,
  vk::Framebuffer& frameBuffer,
  glm::uvec2 extent,
  glm::vec4 clearColor,
  glm::ivec2 offset)
{
  if (!m_created)
  {
    spdlog::error("Pipeline is not built");
    throw std::runtime_error("Pipeline is not built");
  }

  vk::RenderPassBeginInfo renderPassInfo{};
  renderPassInfo.renderPass = m_renderpass.get();
  renderPassInfo.framebuffer = frameBuffer;
  renderPassInfo.renderArea.offset = vk::Offset2D{ offset.x, offset.y };
  renderPassInfo.renderArea.extent = vk::Extent2D{ extent.x, extent.y };

  std::vector<vk::ClearValue> clearValues;

  for (auto& attachment : m_attachments)
  {
    clearValues.push_back({});
  }

  if (m_useDepthAttachment)
  {
    vk::ClearValue cval;
    cval.depthStencil.depth = 1.0;
    clearValues.push_back(cval);
  }

  renderPassInfo.setClearValues(clearValues);

  buf.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  buf.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
}

BG::Pipeline::Pipeline(vk::Device device)
  : m_device(device)
{
  m_vertexInputInfo.setVertexBindingDescriptions(m_bindingDescriptions);
  m_vertexInputInfo.setVertexAttributeDescriptions(m_attributeDescriptions);

  m_inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
  m_inputAssemblyInfo.primitiveRestartEnable = false;

  m_rasterizer.depthClampEnable = false;
  m_rasterizer.rasterizerDiscardEnable = false;
  m_rasterizer.polygonMode = vk::PolygonMode::eFill;
  m_rasterizer.lineWidth = 1.0f;
  m_rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  m_rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
  m_rasterizer.depthBiasEnable = false;
  
  m_multisampling.sampleShadingEnable = false;
  m_multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
}

void BG::Pipeline::InitBackend()
{
  if (!glslangInitialized)
  {
    glslang::InitializeProcess();
    glslangInitialized = true;
  }
}
