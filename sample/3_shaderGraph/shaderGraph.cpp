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


#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

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

  std::string graphFile = SRC_DIR"/sample/3_shaderGraph/graph.json";

  bool reload = false;

  r.Run(
    // Init
    [&]() {
      // Load shader graph
      graph = std::make_shared<ShaderGraph::Graph>(graphFile, r);
    },
    // Render
    [&](Renderer::Context& ctx) {
      int width = r.getWidth(), height = r.getHeight();

      if (reload)
      {
        r.getDevice().waitIdle(); // Wait for all previous frames to finish
        graph = std::make_shared<ShaderGraph::Graph>(graphFile, r);
        reload = false;
      }

      ctx.cmdBuffer.Begin();
      if (graph) graph->Render(r, ctx);
      ctx.cmdBuffer.End();
    },
    // GUI Thread
    [&]() {
      graph->RenderGUI();

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