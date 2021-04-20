#include "pipelines.hpp"
#include "renderer.hpp"
#include "buffer.hpp"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include <SPIRV-Reflect/spirv_reflect.h>

using namespace BG;

std::vector<uint32_t> BuildSPIRV(glslang::TProgram& program, EShLanguage shaderType)
{
  std::vector<unsigned int> spirv;
  spv::SpvBuildLogger logger;
  glslang::SpvOptions spvOptions;

  spvOptions.validate = true;

  glslang::GlslangToSpv(*program.getIntermediate(shaderType), spirv, &logger, &spvOptions);

  return spirv;
}

void BindDescriptorReflection(Pipeline& p, int binding, SpvReflectDescriptorType type, vk::ShaderStageFlags stage, int arraySize = 1, bool unbounded = false)
{
  if (type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
  {
    spdlog::debug("Descriptor: binding = {}, Uniform Buffer", binding);
    p.AddDescriptorUniform(binding, stage, arraySize, unbounded);
  }
  else if (type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
  {
    spdlog::debug("Descriptor: binding = {}, Texture / Combined Sampler", binding);
    p.AddDescriptorTexture(binding, stage, arraySize, unbounded);
  }
}

std::vector<uint32_t> BG::Pipeline::BuildProgramFromSrc(std::string shaders, int _shaderType)
{
  EShLanguage shaderType = EShLanguage(_shaderType);

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
  Resources.limits.nonInductiveForLoops = true;
  Resources.limits.generalUniformIndexing = true;
  Resources.limits.generalSamplerIndexing = true;
  Resources.limits.generalVariableIndexing = true;
  Resources.limits.generalVaryingIndexing = true;

  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

  const int DefaultVersion = 100;

  if (!shader.parse(&Resources, DefaultVersion, false, messages))
  {
    spdlog::error("GLSL Parsing Failed\n{}{}", shader.getInfoLog(), shader.getInfoDebugLog());
    throw std::runtime_error("GLSL Parsing Error");
  }

  glslang::TProgram program;
  program.addShader(&shader);

  if (!program.link(messages))
  {
    spdlog::error("Link failed");
    throw std::runtime_error("GLSL Linking Error");
  }
  
  auto spirv = BuildSPIRV(program, shaderType);

  SpvReflectShaderModule module;
  SpvReflectResult result = spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t), spirv.data(), &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  vk::ShaderStageFlags stage;

  switch (module.shader_stage)
  {
  case (SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT):
    stage = vk::ShaderStageFlagBits::eFragment;
    break;
  case (SPV_REFLECT_SHADER_STAGE_VERTEX_BIT):
    stage = vk::ShaderStageFlagBits::eVertex;
    break;
  default:
    stage = vk::ShaderStageFlagBits::eAll;
    break;
  }

  for (uint32_t i = 0; i < module.descriptor_binding_count; i++)
  {
    SpvReflectDescriptorBinding& binding = module.descriptor_bindings[i];

    bool unbounded = binding.type_description->op == SpvOpTypeRuntimeArray;

    if (std::string(binding.name) != "")
    {
      spdlog::debug("Descriptor name {}, unbounded={}", binding.name, unbounded);
      this->m_name2bindings[binding.name] = binding.binding;
    }

    if (binding.block.members != nullptr)
    {
      for (uint32_t j = 0; j < binding.block.member_count; j++)
      {
        auto& member = binding.block.members[j];

        spdlog::debug("Member variable name {}, offset {}", member.name, member.absolute_offset);
        this->m_name2bindings[member.name] = binding.binding;
        this->m_memberOffsets[member.name] = member.absolute_offset;
      }
    
      this->m_uniformBlockSize[binding.name] = binding.block.padded_size;
      spdlog::debug("Block size {}", binding.block.padded_size);
    }

    BindDescriptorReflection(*this, binding.binding, binding.descriptor_type, stage, 1, unbounded);
  }

  for (uint32_t i = 0; i < module.push_constant_block_count; i++)
  {
    auto& pushConstant = module.push_constant_blocks[i];

    this->AddPushConstant(pushConstant.absolute_offset, pushConstant.padded_size, stage);
  
    spdlog::debug("Push constant {}, offset={}, size={}", i, pushConstant.absolute_offset, pushConstant.padded_size);

    for (uint32_t j = 0; j < pushConstant.member_count; j++)
    {
      auto& member = pushConstant.members[j];

      spdlog::debug("Member variable name {}, offset {}", member.name, member.absolute_offset);
      this->m_memberOffsets[member.name] = member.absolute_offset;
    }
  }

  spvReflectDestroyShaderModule(&module);

  return spirv;
}

vk::UniqueShaderModule BG::Pipeline::AddShaders(std::string shaders, int shaderType)
{
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
  desc.setOffset(uint32_t(offset));

  m_attributeDescriptions.push_back(desc);
}

int BG::Pipeline::GetBindingByName(std::string name)
{
  if (m_name2bindings.find(name) == m_name2bindings.end()) return -1;
  return m_name2bindings[name];
}

uint32_t BG::Pipeline::GetMemberOffset(std::string name)
{
  if (m_memberOffsets.find(name) == m_memberOffsets.end()) return 0xFFFFFFFF;
  return m_memberOffsets[name];
}

void BG::Pipeline::AddDescriptorUniform(int binding, vk::ShaderStageFlags stage, int count, bool unbounded)
{
  vk::DescriptorSetLayoutBinding layoutBinding;
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stage;
  layoutBinding.pImmutableSamplers = nullptr;

  m_descSetLayoutBindings.push_back(layoutBinding);
  if (unbounded)
    m_descSetLayoutBindingFlags.push_back(vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount);
  else
    m_descSetLayoutBindingFlags.push_back(vk::DescriptorBindingFlagBits(0));
}

void BG::Pipeline::AddDescriptorTexture(int binding, vk::ShaderStageFlags stage, int count, bool unbounded)
{
  vk::DescriptorSetLayoutBinding layoutBinding;
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  layoutBinding.descriptorCount = unbounded ? 4096 : count;
  layoutBinding.stageFlags = stage;
  layoutBinding.pImmutableSamplers = nullptr;

  m_descSetLayoutBindings.push_back(layoutBinding);
  if (unbounded)
    m_descSetLayoutBindingFlags.push_back(vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount);
  else
    m_descSetLayoutBindingFlags.push_back(vk::DescriptorBindingFlagBits(0));
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
  vk::DescriptorSetLayoutBindingFlagsCreateInfo layoutFlagsInfo;
  layoutFlagsInfo.setBindingCount(m_descSetLayoutBindings.size());
  layoutFlagsInfo.setBindingFlags(m_descSetLayoutBindingFlags);
  layoutInfo.setBindings(m_descSetLayoutBindings);
  if (r.m_hasDescriptorIndexing)
  {
    layoutInfo.setPNext(&layoutFlagsInfo);
  }

  m_descriptorSetLayout = m_device.createDescriptorSetLayoutUnique(layoutInfo);

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout.get();
  pipelineLayoutInfo.setPushConstantRanges(m_pushConstants);

  m_layout = m_device.createPipelineLayoutUnique(pipelineLayoutInfo);

  std::vector<vk::AttachmentReference> attachments;

  uint32_t attachmentCount;
  for (attachmentCount = 0; attachmentCount < m_attachments.size(); attachmentCount++)
  {
    attachments.push_back({ attachmentCount, vk::ImageLayout::eColorAttachmentOptimal });
  }

  vk::AttachmentReference depthAttachmentRef;

  if (m_useDepthAttachment)
  {
    depthAttachmentRef.attachment = attachmentCount;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
  }

  std::vector<vk::SubpassDescription> subpass;
  vk::SubpassDescription mainSubpass;
  mainSubpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
  mainSubpass.setColorAttachments(attachments);
  if (m_useDepthAttachment) mainSubpass.setPDepthStencilAttachment(&depthAttachmentRef);
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

  for (int i = 0; i < m_attachments.size(); i++)
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
  
  auto result = m_device.createGraphicsPipelineUnique(nullptr, pipelineInfo, nullptr);

  if (result.result != vk::Result::eSuccess) throw std::runtime_error("Create pipeline failed");

  m_pipeline = std::move(result.value);

  m_created = true;
}

void BG::Pipeline::AddPushConstant(uint32_t offset, uint32_t size, vk::ShaderStageFlags stage)
{
  vk::PushConstantRange range;
  range.offset = offset;
  range.size = size;
  range.stageFlags = stage;

  m_pushConstants.push_back(range);
}

vk::DescriptorSet Pipeline::AllocDescSet(vk::DescriptorPool pool, int variableDescriptorCount)
{
  uint32_t vCount = variableDescriptorCount;

  vk::DescriptorSetVariableDescriptorCountAllocateInfoEXT variableCount;
  variableCount.pDescriptorCounts = &vCount;
  variableCount.descriptorSetCount = 1;

  vk::DescriptorSetAllocateInfo allocInfo;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_descriptorSetLayout.get();

  if (variableDescriptorCount != 0) allocInfo.pNext = &variableCount;

  return m_device.allocateDescriptorSets(allocInfo)[0];
}


void BG::Pipeline::BindGraphicsUniformBuffer(Pipeline& p, vk::DescriptorSet descSet, const BG::Buffer& buffer, uint32_t offset, uint32_t range, int binding, int arrayElement)
{
  vk::DescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = buffer.buffer;
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

  for (int i = 0; i < m_attachments.size(); i++)
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

BG::Pipeline::Pipeline(Renderer& r, vk::Device device)
  : r(r), m_device(device)
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
