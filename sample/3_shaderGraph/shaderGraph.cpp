#include "berkeley_gfx.hpp"
#include "renderer.hpp"
#include "pipelines.hpp"
#include "command_buffer.hpp"
#include "buffer.hpp"
#include "texture_system.hpp"

#include <string>
#include <fstream>
#include <streambuf>

#include <imgui.h>

#include <json.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <filesystem>

using namespace BG;

struct ShaderUniform
{
  glm::vec4 iMouse;
  glm::vec3 iResolution;
  float iTime;
  float iTimeDelta;
  float iFrame;
};

size_t ShaderUniformSize = sizeof(ShaderUniform) % 0x40 > 0 ? (sizeof(ShaderUniform) / 0x40 + 1) * 0x40 : sizeof(ShaderUniform);

struct Parameter
{
  std::string name;
  uint32_t binding;

  virtual void RenderGUI()
  {
    if (ImGui::TreeNodeEx(this, 0, "%s (Unknown type)", name.data()))
    {
      ImGui::TreePop();
    }
  }
};

struct FloatParameter : Parameter
{
  float min;
  float max;
  float defaultValue;
  float value;

  virtual void RenderGUI()
  {
    if (ImGui::TreeNodeEx(this, 0, "%s (float)", name.data()))
    {
      ImGui::SliderFloat("", &value, min, max);
      ImGui::TreePop();
    }
  }
};

struct Vec3Parameter : Parameter
{
  glm::vec3 min;
  glm::vec3 max;
  glm::vec3 defaultValue;
  glm::vec3 value;

  virtual void RenderGUI()
  {
    if (ImGui::TreeNodeEx(this, 0, "%s (vec3)", name.data()))
    {
      ImGui::SliderFloat("X", &value.x, min.x, max.x);
      ImGui::SliderFloat("Y", &value.y, min.y, max.y);
      ImGui::SliderFloat("Z", &value.z, min.z, max.z);
      ImGui::TreePop();
    }
  }
};

struct Texture
{
  std::string name;

  glm::uvec2 extent;
  vk::Format format;

  std::vector<std::shared_ptr<Image>> image;
  std::vector<vk::UniqueImageView> imageView;
};

struct TextureBinding
{
  std::string name;
  uint32_t binding;
};

struct Stage
{
  std::string name;
  std::string shaderFile;
  std::vector<std::shared_ptr<Parameter>> parameters;

  int builtinParamBindPoint;
  
  std::shared_ptr<Texture> output;
  std::vector<TextureBinding> texture;
  
  std::shared_ptr<Pipeline> pipeline;
};

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

class ShaderGraph
{
private:
  std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
  std::unordered_map<std::string, std::shared_ptr<Stage>> stages;
  std::unordered_map<std::string, std::string> dependency; // key: output name, value: stage name

  std::string outputStage;

  std::shared_ptr<Buffer> uniformBuffer;

  std::chrono::steady_clock::time_point startTime, lastTime;
  uint32_t frameCount = 0;

public:
  ShaderGraph(std::string jsonFile, Renderer& r)
  {
    // Allocate a constants buffer
    uniformBuffer = r.getMemoryAllocator()->AllocCPU2GPU(ShaderUniformSize * r.getSwapchainImageViews().size(), vk::BufferUsageFlagBits::eUniformBuffer);

    using json = nlohmann::json;

    std::ifstream f(jsonFile);
    std::string fileContent = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    spdlog::debug("Read ShaderGraph JSON {}", fileContent);

    json j = json::parse(fileContent);

    if (!j.is_object())
    {
      spdlog::error("JSON file read is not an valid object {}", j);
      return;
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
          auto param = std::make_shared<FloatParameter>();
          jsonParam.at("name").get_to(param->name);
          jsonParam.at("min").get_to(param->min);
          jsonParam.at("max").get_to(param->max);
          jsonParam.at("default").get_to(param->defaultValue);
          param->value = param->defaultValue;

          stage->parameters.push_back(param);
        }
        else if (type == "vec3")
        {
          auto param = std::make_shared<Vec3Parameter>();
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
          
          stage->parameters.push_back(param);
        }
      }

      // Load in textures (sampled)
      for (std::string texture : jsonStage["textures"])
      {
        stage->texture.push_back(TextureBinding{ texture, 0 });
      }

      // Load in shader text & construct pipeline & ouput texture/view
      {
        std::filesystem::path jsonPath = jsonFile;
        jsonPath.remove_filename();

        std::ifstream f(jsonPath.append(stage->shaderFile));
        std::string shaderText = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        spdlog::debug("Shader stage {} src: {}", stage->name, shaderText);

        stage->pipeline = r.CreatePipeline();
        stage->pipeline->AddFragmentShaders(shaderText);
        stage->pipeline->AddVertexShaders(fullscreenVertexShader);
        stage->pipeline->SetViewport(float(r.getWidth()), float(r.getHeight()));

        for (std::string outputName : jsonStage["output"])
        {
          vk::Format format = r.getSwapChainFormat();

          glm::uvec2 extent = glm::uvec2(r.getWidth(), r.getHeight());

          auto texture = std::make_shared<Texture>();

          for (int i = 0; i < r.getSwapchainImages().size(); i++)
          {
            auto image = r.getMemoryAllocator()->AllocImage2D(extent, 1, format, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::ImageLayout::eUndefined);

            texture->image.push_back(image);

            vk::ImageViewCreateInfo viewInfo;
            viewInfo.image = image->image;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            texture->imageView.push_back(r.getDevice().createImageViewUnique(viewInfo));
            texture->extent = extent;
            texture->format = format;
            texture->name = outputName;
          }

          this->textures[outputName] = texture;
          this->dependency[outputName] = stage->name;

          if (outputName == "framebuffer")
            stage->pipeline->AddAttachment(r.getSwapChainFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
          else
            stage->pipeline->AddAttachment(r.getSwapChainFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        stage->pipeline->BuildPipeline();

        // Map bindings
        for (auto& textureBinding : stage->texture)
        {
          textureBinding.binding = stage->pipeline->GetBindingByName(textureBinding.name);
        }

        stage->builtinParamBindPoint = stage->pipeline->GetBindingByName("iTime");
      }
    }

    startTime = std::chrono::steady_clock::now();
  }

  void Render(Renderer& r, Renderer::Context& ctx, std::string target)
  {
    std::vector<vk::ImageView> renderTarget;

    auto texture = this->textures[target];

    if (target == "framebuffer")
    {
      renderTarget.push_back(ctx.imageView);
    }
    else
    {
      renderTarget.push_back(texture->imageView[ctx.imageIndex].get());
    }

    std::string stageName = this->dependency[target];
    auto stage = this->stages[stageName];

    // Graph dependency
    for (auto& textureBinding : stage->texture)
    {
      if (textureBinding.name.rfind("previous_") != 0)
        Render(r, ctx, textureBinding.name);
    }

    // Render current node
    auto pipeline = stage->pipeline;

    // Allocate descriptor sets & bind uniforms
    auto descSet = pipeline->AllocDescSet(ctx.descPool);
    
    if (stage->builtinParamBindPoint >= 0)
      pipeline->BindGraphicsUniformBuffer(*pipeline, descSet, uniformBuffer, ShaderUniformSize * ctx.imageIndex, sizeof(ShaderUniform), stage->builtinParamBindPoint);

    for (auto& textureBinding : stage->texture)
    {
      int imageIndex = ctx.imageIndex;
      std::string textureName = textureBinding.name;

      if (textureName.rfind("previous_") == 0)
      {
        textureName = textureName.substr(9);
        int size = r.getSwapchainImages().size();
        imageIndex = (imageIndex - 1 + size) % size;
      }

      ctx.cmdBuffer.ImageTransition(this->textures[textureName]->image[imageIndex], vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

      pipeline->BindGraphicsImageView(
        *pipeline, descSet,
        this->textures[textureName]->imageView[imageIndex].get(),
        vk::ImageLayout::eShaderReadOnlyOptimal, r.getTextureSystem()->GetSampler(),
        textureBinding.binding);
    }

    ctx.cmdBuffer.WithRenderPass(*pipeline, renderTarget, texture->extent, [&]() {
      // Bind the pipeline to use
      ctx.cmdBuffer.BindPipeline(*pipeline);
      // Bind the descriptor sets (uniform buffer, texture, etc.)
      ctx.cmdBuffer.BindGraphicsDescSets(*pipeline, descSet);
      // Draw terrain
      ctx.cmdBuffer.Draw(3);
      });

    if (target != "framebuffer")
    {
      ctx.cmdBuffer.ImageTransition(texture->image[ctx.imageIndex], vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
  }

  void Render(Renderer& r, Renderer::Context& ctx)
  {
    // Map & upload the constants
    auto now = std::chrono::steady_clock::now();
    uint8_t* uniformBufferGPU = uniformBuffer->Map<uint8_t>();
    auto& uniform = *(ShaderUniform*)(uniformBufferGPU + ShaderUniformSize * ctx.imageIndex);
    uniform.iResolution = glm::vec3(r.getWidth(), r.getHeight(), 1);
    uniform.iTime = (now - startTime).count() * 1e-9;
    uniform.iMouse = glm::vec4(0.0);
    uniform.iTimeDelta = (now - lastTime).count() * 1e-9;
    uniform.iFrame = frameCount;
    uniformBuffer->UnMap();
    lastTime = now;
    frameCount++;

    Render(r, ctx, "framebuffer");
  }
  
  void RenderGUI()
  {
    ImGui::Begin("Shader Graph");

    for (auto pair : stages)
    {
      auto stage = pair.second; // first: key, second: shader stage
      auto name = pair.first;

      if (ImGui::TreeNodeEx(stage.get(), 0, "Stage %s", name.data()))
      {
        ImGui::Text("Shader File: %s", stage->shaderFile.data());

        for (auto param : stage->parameters)
        {
          param->RenderGUI();
        }

        ImGui::TreePop();
      }
    }

    ImGui::End();
  }
};

// Main function
int main(int, char**)
{
  spdlog::set_level(spdlog::level::debug);

  // Instantiate the Berkeley Gfx renderer & the backend
  Renderer r("Sample Project - Shader Graph", true);

  Pipeline::InitBackend();

  std::shared_ptr<ShaderGraph> graph;

  r.Run(
    // Init
    [&]() {
      // Load shader graph
      graph = std::make_shared<ShaderGraph>(SRC_DIR"/sample/3_shaderGraph/graph.json", r);
    },
    // Render
    [&](Renderer::Context& ctx) {
      int width = r.getWidth(), height = r.getHeight();

      ctx.cmdBuffer.Begin();
      graph->Render(r, ctx);
      ctx.cmdBuffer.End();
    },
    // GUI Thread
    [&]() {
      graph->RenderGUI();
    },
    // Cleanup
    [&]() {

    }
    );
}