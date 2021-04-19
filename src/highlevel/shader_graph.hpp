#pragma once

#include "berkeley_gfx.hpp"

#include "renderer.hpp"

#include <vulkan/vulkan.hpp>

namespace BG::ShaderGraph
{
  struct ShaderUniform
  {
    glm::vec4 iMouse;
    glm::vec3 iResolution;
    float iTime;
    float iTimeDelta;
    int iFrame;
  };

  struct Parameter
  {
    std::string name;
    uint32_t binding;

    virtual void RenderGUI();
    virtual void PushParameter(BG::CommandBuffer& cmdBuf, BG::Pipeline& p);

    virtual ~Parameter() {}
  };

  struct FloatParameter : Parameter
  {
    float min;
    float max;
    float defaultValue;
    float value;

    virtual void RenderGUI();
    virtual void PushParameter(BG::CommandBuffer& cmdBuf, BG::Pipeline& p);
  };

  struct Vec3Parameter : Parameter
  {
    glm::vec3 min;
    glm::vec3 max;
    glm::vec3 defaultValue;
    glm::vec3 value;

    virtual void RenderGUI();
    virtual void PushParameter(BG::CommandBuffer& cmdBuf, BG::Pipeline& p);
  };

  struct Texture
  {
    std::string name;

    glm::uvec2 extent;
    vk::Format format;

    std::vector<std::unique_ptr<BG::Image>> image;
    std::vector<vk::ImageView> imageView;

    bool isInternal = true;
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
    std::vector<std::unique_ptr<Parameter>> parameters;

    int builtinParamBindPoint;

    std::weak_ptr<Texture> output;
    std::vector<TextureBinding> texture;

    std::unique_ptr<BG::Pipeline> pipeline;
  };

  class Graph
  {
  private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    std::unordered_map<std::string, std::shared_ptr<Stage>> stages;
    std::unordered_map<std::string, std::string> dependency; // key: output name, value: stage name

    BG::Renderer& r;

    std::string outputStage;

    BG::Buffer* uniformBuffer;

    std::chrono::steady_clock::time_point startTime, lastTime;
    uint32_t frameCount = 0;

    void CreateTexture(glm::uvec2 extent, vk::Format format, Renderer& r, std::string name);

  public:
    Graph(std::string jsonFile, BG::Renderer& r);
    ~Graph();

    void Render(BG::Renderer& r, BG::Renderer::Context& ctx, std::string target);

    void Render(BG::Renderer& r, BG::Renderer::Context& ctx);

    void RenderGUI();
  };
}