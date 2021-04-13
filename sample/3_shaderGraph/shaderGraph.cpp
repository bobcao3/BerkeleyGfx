#include "berkeley_gfx.hpp"
#include "renderer.hpp"
#include "pipelines.hpp"
#include "command_buffer.hpp"
#include "buffer.hpp"
#include "texture_system.hpp"
#include "shader_graph.hpp"

#include <string>
#include <fstream>
#include <streambuf>

#include <imgui.h>

#include <json.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <filesystem>

using namespace BG;

// Main function
int main(int, char**)
{
  spdlog::set_level(spdlog::level::debug);

  // Instantiate the Berkeley Gfx renderer & the backend
  Renderer r("Sample Project - Shader Graph", true);

  Pipeline::InitBackend();

  std::shared_ptr<ShaderGraph::Graph> graph;

  std::string graphFile = SRC_DIR"/sample/3_shaderGraph/2_customTexture/graph.json";

  bool reload = false;

  r.Run(
    // Init
    [&]() {
      // Load shader graph
      try
      {
        graph = std::make_shared<ShaderGraph::Graph>(graphFile, r);
      }
      catch (const std::runtime_error& error)
      {
        graph = nullptr;
        spdlog::error("Shader load failed {}", error.what());
      }
    },
    // Render
    [&](Renderer::Context& ctx) {
      if (reload)
      {
        r.getDevice().waitIdle(); // Wait for all previous frames to finish

        try
        {
          graph = std::make_shared<ShaderGraph::Graph>(graphFile, r);
        }
        catch (const std::runtime_error& error)
        {
          graph = nullptr;
          spdlog::error("Shader reload failed {}", error.what());
        }

        reload = false;
      }

      ctx.cmdBuffer.Begin();
      if (graph)
        graph->Render(r, ctx);
      else
        ctx.cmdBuffer.ImageTransition(ctx.image, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor);
      ctx.cmdBuffer.End();
    },
    // GUI Thread
    [&]() {
      if (graph)
        graph->RenderGUI();
      else
      {
        ImGui::SetNextWindowPos(ImVec2(5, 5));
        ImGui::SetNextWindowSize(ImVec2(400, 50));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.8f, 0.2f, 0.1f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::Begin("Error", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::Text("No shaders loaded. Check console output for errors");
        ImGui::Text("Click reload to try reload");
        ImGui::End();
        ImGui::PopStyleColor(2);
      }

      if (ImGui::Button("Reload Shaders"))
      {
        reload = true; // We want to reload on the main thread
      }
    },
    // Cleanup
    [&]() {
    }
    );
}