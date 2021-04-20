#include "shader_graph.hpp"

#include "texture_system.hpp"
#include "command_buffer.hpp"
#include "pipelines.hpp"
#include "buffer.hpp"

#include <json.hpp>
#include <imgui/imgui.h>
#include <fstream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

using namespace BG;
using namespace BG::ShaderGraph;

void Parameter::RenderGUI()
{
  if (ImGui::TreeNodeEx(this, 0, "%s (Unknown type)", name.data()))
  {
    ImGui::TreePop();
  }
}

void Parameter::PushParameter(CommandBuffer& cmdBuf, Pipeline& p) {}


void FloatParameter::RenderGUI()
{
  if (ImGui::TreeNodeEx(this, 0, "%s (float)", name.data()))
  {
    ImGui::SliderFloat("", &value, min, max);
    ImGui::TreePop();
  }
}

void FloatParameter::PushParameter(CommandBuffer& cmdBuf, Pipeline& p)
{
  cmdBuf.PushConstants(p, vk::ShaderStageFlagBits::eFragment, p.GetMemberOffset(name), value);
}

void Vec3Parameter::RenderGUI()
{
  if (ImGui::TreeNodeEx(this, 0, "%s (vec3)", name.data()))
  {
    ImGui::SliderFloat("X", &value.x, min.x, max.x);
    ImGui::SliderFloat("Y", &value.y, min.y, max.y);
    ImGui::SliderFloat("Z", &value.z, min.z, max.z);
    ImGui::TreePop();
  }
}

void Vec3Parameter::PushParameter(CommandBuffer& cmdBuf, Pipeline& p)
{
  cmdBuf.PushConstants(p, vk::ShaderStageFlagBits::eFragment, p.GetMemberOffset(name), value);
}

std::string fullscreenVertexShader = R"V0G0N(

#version 450

vec2 vertex[3] = vec2[](
  vec2(-1.0, -1.0),
  vec2(-1.0, 4.0),
  vec2(4.0, -1.0)
);

layout(location = 0) out vec2 UV;

void main() {
  gl_Position = vec4(vertex[gl_VertexIndex], 0.0, 1.0);
  UV = vertex[gl_VertexIndex] * 0.5 + 0.5;
}

)V0G0N";

void BG::ShaderGraph::Graph::CreateTexture(glm::uvec2 extent, vk::Format format, Renderer& r, std::string name)
{
  auto texture = std::make_shared<Texture>();

  for (int i = 0; i < r.getSwapchainImages().size(); i++)
  {
    auto image = r.getMemoryAllocator().AllocImage2D(extent, 1, format, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::ImageLayout::eUndefined);

    auto _cmdBuf = r.AllocCmdBuffer();
    CommandBuffer cmdBuf(r.getDevice(), _cmdBuf.get(), r.getTracker());

    cmdBuf.Begin();
    cmdBuf.ImageTransition(*image, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
    cmdBuf.End();

    r.SubmitCmdBufferNow(cmdBuf.GetVkCmdBuf());

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image->image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    texture->image.push_back(std::move(image));
    texture->imageView.push_back(r.getDevice().createImageView(viewInfo));
    texture->extent = extent;
    texture->format = format;
    texture->name = name;
  }

  this->textures[name] = texture;
}

Graph::Graph(std::string jsonFile, Renderer& r)
  : r(r)
{
  using json = nlohmann::json;

  std::ifstream f(jsonFile);
  std::string fileContent = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  f.close();

  spdlog::debug("Read ShaderGraph JSON, file = {}", jsonFile);

  json j = json::parse(fileContent);

  if (!j.is_object())
  {
    spdlog::error("JSON file read is not an valid object {}", j);
    return;
  }

  std::filesystem::path jsonPath = jsonFile;
  jsonPath.remove_filename();

  // Load custom textures
  for (auto jsonPairTexture : j["images"].items())
  {
    std::string name = jsonPairTexture.key();
    auto image = jsonPairTexture.value();

    if (image.find("fileName") != image.end())
    {
      std::filesystem::path p = jsonPath;
      p.append(std::string(image["fileName"]));
      auto pathString = p.string();

      int imageWidth, imageHeight, channels;
      uint8_t* imgData = stbi_load(pathString.data(), &imageWidth, &imageHeight, &channels, 0);

      auto handle = r.getTextureSystem().AddTexture(imgData, imageWidth, imageHeight, imageWidth * imageHeight * channels, vk::Format::eR8G8B8A8Srgb);

      auto texture = std::make_shared<Texture>();
      texture->format = vk::Format::eR8G8B8A8Srgb;
      texture->extent = glm::uvec2(imageWidth, imageHeight);
      texture->isInternal = false;

      for (int i = 0; i < r.getSwapchainImages().size(); i++)
      {
        texture->imageView.push_back(r.getTextureSystem().GetImageView(handle));
      }

      textures[name] = texture;

      continue;
    }

    glm::uvec2 extent = glm::uvec2(r.getWidth(), r.getHeight());
    vk::Format format = r.getSwapChainFormat();

    if (image.find("resolution") != image.end())
    {
      image.at("resolution")[0].get_to(extent.x);
      image.at("resolution")[1].get_to(extent.y);
    }

    if (image.find("format") != image.end())
    {
      std::string formatName = image.at("format");

      if (formatName == "r8")
      {
        format = vk::Format::eR8Unorm;
      }
      else if (formatName == "rg8")
      {
        format = vk::Format::eR8G8Unorm;
      }
      else if (formatName == "rgb8")
      {
        format = vk::Format::eR8G8B8Unorm;
      }
      else if (formatName == "rgba8")
      {
        format = vk::Format::eR8G8B8A8Unorm;
      }
      else if (formatName == "r16")
      {
        format = vk::Format::eR16Unorm;
      }
      else if (formatName == "rg16")
      {
        format = vk::Format::eR16G16Unorm;
      }
      else if (formatName == "rgb16")
      {
        format = vk::Format::eR16G16B16Unorm;
      }
      else if (formatName == "rgba16")
      {
        format = vk::Format::eR16G16B16A16Unorm;
      }
      else if (formatName == "r32f")
      {
        format = vk::Format::eR32Sfloat;
      }
      else if (formatName == "rg32f")
      {
        format = vk::Format::eR32G32Sfloat;
      }
      else if (formatName == "rgb32f")
      {
        format = vk::Format::eR32G32B32Sfloat;
      }
      else if (formatName == "rgba32f")
      {
        format = vk::Format::eR32G32B32A32Sfloat;
      }
    }

    spdlog::debug("Texture image {}, resolution={}x{}, format={}", name, extent.x, extent.y, format);

    CreateTexture(extent, format, r, name);
  }

  // Load in stages
  for (auto jsonPairStage : j["stages"].items())
  {
    auto stage = std::make_shared<Stage>();
    stage->name = jsonPairStage.key();

    auto jsonStage = jsonPairStage.value();
    jsonStage.at("shader").get_to(stage->shaderFile);

    stages[stage->name] = stage;

    spdlog::debug("Load stage {}, shader file {}", stage->name, stage->shaderFile);

    // Load in parameters
    for (auto jsonParam : jsonStage["parameters"])
    {
      std::string type = jsonParam.at("type");
      spdlog::debug("Parameter type {}", type);
      if (type == "float")
      {
        auto param = std::make_unique<FloatParameter>();
        jsonParam.at("name").get_to(param->name);
        jsonParam.at("min").get_to(param->min);
        jsonParam.at("max").get_to(param->max);
        jsonParam.at("default").get_to(param->defaultValue);
        param->value = param->defaultValue;

        stage->parameters.push_back(std::move(param));
      }
      else if (type == "vec3")
      {
        auto param = std::make_unique<Vec3Parameter>();
        jsonParam.at("name").get_to(param->name);

        jsonParam.at("min")[0].get_to(param->min.x);
        jsonParam.at("min")[1].get_to(param->min.y);
        jsonParam.at("min")[2].get_to(param->min.z);

        jsonParam.at("max")[0].get_to(param->max.x);
        jsonParam.at("max")[1].get_to(param->max.y);
        jsonParam.at("max")[2].get_to(param->max.z);

        jsonParam.at("default")[0].get_to(param->defaultValue.x);
        jsonParam.at("default")[1].get_to(param->defaultValue.y);
        jsonParam.at("default")[2].get_to(param->defaultValue.z);

        param->value = param->defaultValue;

        stage->parameters.push_back(std::move(param));
      }
    }

    // Load in textures (sampled)
    for (std::string texture : jsonStage["textures"])
    {
      stage->texture.push_back(TextureBinding{ texture, 0 });
    }

    // Load in shader text & construct pipeline & ouput texture/view
    {
      auto shaderPath = jsonPath;
      shaderPath.append(stage->shaderFile);
      std::ifstream f(shaderPath);
      std::string shaderText = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      f.close();

      spdlog::debug("Shader stage {}", stage->name);

      glm::uvec2 extent = glm::uvec2(r.getWidth(), r.getHeight());

      stage->pipeline = r.CreatePipeline();
      stage->pipeline->AddFragmentShaders(shaderText);
      stage->pipeline->AddVertexShaders(fullscreenVertexShader);

      for (std::string outputName : jsonStage["output"])
      {
        vk::Format format = r.getSwapChainFormat();
        
        if (this->textures.find(outputName) == this->textures.end())
        {
          CreateTexture(extent, format, r, outputName);
        }
        else
        {
          extent = this->textures[outputName]->extent;
          format = this->textures[outputName]->format;
        }

        this->dependency[outputName] = stage->name;

        if (outputName == "framebuffer")
          stage->pipeline->AddAttachment(format, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
        else
          stage->pipeline->AddAttachment(format, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
      }

      stage->pipeline->SetViewport(float(extent.x), float(extent.y));
      stage->pipeline->BuildPipeline();

      // Map bindings
      for (auto& textureBinding : stage->texture)
      {
        textureBinding.binding = stage->pipeline->GetBindingByName(textureBinding.name);
      }

      stage->builtinParamBindPoint = stage->pipeline->GetBindingByName("iTime");
    }
  
    // Verify all bindings
    for (auto textureBinding : stage->texture)
    {
      if (textureBinding.binding > 1024)
      {
        spdlog::error("Bad texture binding! Check whether the uniform name matches the name in the JSON file ({} in stage {})", textureBinding.name, stage->name);
        throw std::runtime_error("Bad binding");
      }
    }

  }
  
  startTime = std::chrono::steady_clock::now();
}

BG::ShaderGraph::Graph::~Graph()
{
  for (auto pair : textures)
  {
    if (pair.second->isInternal)
    {
      for (auto imageView : pair.second->imageView)
      {
        r.getDevice().destroyImageView(imageView);
      }
    }
  }
}

void Graph::Render(Renderer& r, Renderer::Context& ctx, std::string target)
{
  std::vector<vk::ImageView> renderTarget;

  auto texture = this->textures[target];

  if (target == "framebuffer")
  {
    renderTarget.push_back(ctx.imageView);
  }
  else
  {
    renderTarget.push_back(texture->imageView[ctx.imageIndex]);
  }

  std::string stageName = this->dependency[target];
  auto stage = this->stages[stageName];

  // Graph dependency
  for (auto& textureBinding : stage->texture)
  {
    if (textureBinding.name.rfind("previous_") != 0)
    {
      if (textures[textureBinding.name]->isInternal)
      {
        Render(r, ctx, textureBinding.name);
      }
    }
  }

  // Render current node
  auto& pipeline = stage->pipeline;

  // Allocate descriptor sets & bind uniforms
  auto descSet = pipeline->AllocDescSet(ctx.descPool);

  if (stage->builtinParamBindPoint >= 0)
    pipeline->BindGraphicsUniformBuffer(*pipeline, descSet, *uniformBuffer, 0, uint32_t(sizeof(ShaderUniform)), stage->builtinParamBindPoint);

  for (auto& textureBinding : stage->texture)
  {
    int imageIndex = ctx.imageIndex;
    std::string textureName = textureBinding.name;

    if (textureName.rfind("previous_") == 0)
    {
      textureName = textureName.substr(9);
      int size = int(r.getSwapchainImages().size());
      imageIndex = (imageIndex - 1 + size) % size;
    }

    if (textures[textureName]->isInternal)
    {
      ctx.cmdBuffer.ImageTransition(
        *textures[textureName]->image[imageIndex],
        vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eFragmentShader,
        vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    pipeline->BindGraphicsImageView(
      *pipeline, descSet,
      textures[textureName]->imageView[imageIndex],
      vk::ImageLayout::eShaderReadOnlyOptimal, r.getTextureSystem().GetSampler(),
      textureBinding.binding);
  }

  if (target != "framebuffer")
  {
    ctx.cmdBuffer.ImageTransition(
      *texture->image[ctx.imageIndex],
      vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
  }

  ctx.cmdBuffer.WithRenderPass(*pipeline, renderTarget, texture->extent, [&]() {
    // Bind the pipeline to use
    ctx.cmdBuffer.BindPipeline(*pipeline);
    // Bind the descriptor sets (uniform buffer, texture, etc.)
    ctx.cmdBuffer.BindGraphicsDescSets(*pipeline, descSet);
    // Push parameters as push constants
    for (auto& p : stage->parameters)
    {
      p->PushParameter(ctx.cmdBuffer, *pipeline);
    }
    // Draw full-screen
    ctx.cmdBuffer.Draw(3);
    });

  if (target != "framebuffer")
  {
    ctx.cmdBuffer.ImageTransition(*texture->image[ctx.imageIndex], vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
  }
}

void Graph::Render(Renderer& r, Renderer::Context& ctx)
{
  // Map & upload the constants
  uniformBuffer = r.getMemoryAllocator().AllocTransient(sizeof(ShaderUniform), vk::BufferUsageFlagBits::eUniformBuffer);
  auto now = std::chrono::steady_clock::now();
  ShaderUniform* uniformBufferGPU = uniformBuffer->Map<ShaderUniform>();
  uniformBufferGPU->iResolution = glm::vec3(r.getWidth(), r.getHeight(), 1.0f);
  uniformBufferGPU->iTime = float((now - startTime).count() * 1e-9);
  uniformBufferGPU->iMouse = glm::vec4(r.getCursorPos(), 0.0f, 0.0f);
  uniformBufferGPU->iTimeDelta = float((now - lastTime).count() * 1e-9);
  uniformBufferGPU->iFrame = int(frameCount);
  uniformBuffer->UnMap();
  lastTime = now;
  frameCount++;

  Render(r, ctx, "framebuffer");
}

void Graph::RenderGUI()
{
  ImGui::Begin("Shader Graph");

  for (auto pair : stages)
  {
    auto stage = pair.second; // first: key, second: shader stage
    auto name = pair.first;

    if (ImGui::TreeNodeEx(stage.get(), ImGuiTreeNodeFlags_CollapsingHeader, "Stage %s", name.data()))
    {
      ImGui::Text("Shader File: %s", stage->shaderFile.data());

      for (auto& param : stage->parameters)
      {
        param->RenderGUI();
      }

      //ImGui::TreePop();
    }
  }

  ImGui::End();
}