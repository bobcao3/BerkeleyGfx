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

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <stb/stb_image.h>

using namespace BG;

// String for storing the shaders
std::string vertexShader;
std::string fragmentShader;

// Terrain vertex format
struct Vertex {
  glm::vec3 pos; // Always a [0..1] grid so that we can map the height map
};

// Shader uniform buffer format
struct ShaderUniform
{
  glm::mat4 viewProjMtx;
};

// Our CPU side index / vertex buffer
std::vector<Vertex> vertices;
std::vector<uint32_t> indicies;

// Read the shaders into string from file
void load_shader_file()
{
  std::ifstream tf(SRC_DIR"/sample/2_terrain/fragment.glsl");
  fragmentShader = std::string((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());

  std::ifstream tv(SRC_DIR"/sample/2_terrain/vertex.glsl");
  vertexShader = std::string((std::istreambuf_iterator<char>(tv)), std::istreambuf_iterator<char>());
}

void generateGrid(int width)
{
  // Vertices
  for (int i = 0; i <= width; i++)
  {
    for (int j = 0; j <= width; j++)
    {
      glm::vec3 pos = glm::vec3(i, 0, j) / float(width);
      vertices.push_back({ pos });
    }
  }

  // Triangles
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < width; j++)
    {
      int index00 = i * (width + 1) + j;
      int index01 = i * (width + 1) + (j + 1);
      int index10 = (i + 1) * (width + 1) + j;
      int index11 = (i + 1) * (width + 1) + (j + 1);

      // first triangle (CCW)
      indicies.push_back(index01); indicies.push_back(index10); indicies.push_back(index00);
      // second triangle
      indicies.push_back(index11); indicies.push_back(index10); indicies.push_back(index01);
    }
  }
}

// Main function
int main(int, char**)
{
  spdlog::set_level(spdlog::level::debug);

  // Load the shader file into string
  load_shader_file();

  // Instantiate the Berkeley Gfx renderer & the backend
  Renderer r("Sample Project - glTF Viewer", true);

  Pipeline::InitBackend();

  std::shared_ptr<Pipeline> pipeline;

  // Our GPU buffers holding the vertices and the indices
  std::shared_ptr<Buffer> vertexBuffer, indexBuffer, uniformBuffer;

  BG::VertexBufferBinding vertexBinding;

  // Camera control parameters
  glm::vec3 cameraLookAt = glm::vec3(5.0, 0.5, 5.0);
  float cameraOrbitRadius = 5.0;
  float cameraOrbitHeight = 1.5;

  glm::mat4 terrainTransform = glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));

  // Load the height map PNG
  int heightmapWidth, heightmapHeight, channels;
  uint8_t* heightmapImg = stbi_load(SRC_DIR"/sample/2_terrain/heightmap.png", &heightmapWidth, &heightmapHeight, &channels, 0);

  // Generate the grid for rendering terrain
  generateGrid(heightmapWidth);

  r.Run(
    // Init
    [&]() {
      // Upload texture to GPU
      r.getTextureSystem()->AddTexture(heightmapImg, heightmapWidth, heightmapHeight, channels * heightmapHeight * heightmapWidth, vk::Format::eR8G8B8A8Unorm);

      // Allocate a vertex buffer on GPU, and upload our buffer
      vertexBuffer = r.getMemoryAllocator()->AllocCPU2GPU(vertices.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      Vertex* vertexBufferGPU = vertexBuffer->Map<Vertex>();
      std::copy(vertices.begin(), vertices.end(), vertexBufferGPU);
      vertexBuffer->UnMap();

      // Allocate a index buffer, and upload our data
      indexBuffer = r.getMemoryAllocator()->AllocCPU2GPU(indicies.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      uint32_t* indexBufferGPU = indexBuffer->Map<uint32_t>();
      std::copy(indicies.begin(), indicies.end(), indexBufferGPU);
      indexBuffer->UnMap();

      // Allocate a constants buffer
      //uniformBuffer = r.getMemoryAllocator()->AllocCPU2GPU(sizeof(ShaderUniform) * r.getSwapchainImageViews().size(), vk::BufferUsageFlagBits::eUniformBuffer);

      // Create a empty pipline
      pipeline = r.CreatePipeline();
      // Add a vertex binding
      vertexBinding = pipeline->AddVertexBuffer<Vertex>();
      // Specify two vertex input attributes from the binding
      pipeline->AddAttribute(vertexBinding, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
      // Add shaders
      pipeline->AddFragmentShaders(fragmentShader);
      pipeline->AddVertexShaders(vertexShader);
      // Set the viewport
      pipeline->SetViewport(float(r.getWidth()), float(r.getHeight()));
      // Add an attachment for the pipeline to render to
      pipeline->AddAttachment(r.getSwapChainFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
      pipeline->AddDepthAttachment();
      // Build the pipeline
      pipeline->BuildPipeline();
    },
    // Render
    [&](Renderer::Context& ctx) {
      int width = r.getWidth(), height = r.getHeight();

      // Prepare uniform buffer (view & projection matrix)
      glm::mat4 viewMtx = glm::lookAt(glm::vec3(cos(ctx.time * 0.2) * cameraOrbitRadius, cameraOrbitHeight, sin(ctx.time * 0.2) * cameraOrbitRadius) + cameraLookAt, cameraLookAt, glm::vec3(0.0, 1.0, 0.0));
      glm::mat4 projMtx = glm::perspective(glm::radians(45.0f), float(width) / float(height), 0.1f, 256.0f);
      projMtx[1][1] *= -1.0;

      // Map & upload the constants
      uniformBuffer = r.getMemoryAllocator()->AllocTransient(sizeof(ShaderUniform), vk::BufferUsageFlagBits::eUniformBuffer);
      ShaderUniform* uniformBufferGPU = uniformBuffer->Map<ShaderUniform>();
      uniformBufferGPU->viewProjMtx = projMtx * viewMtx;
      uniformBuffer->UnMap();

      // Allocate descriptor sets & bind uniforms
      auto descSet = pipeline->AllocDescSet(ctx.descPool);
      pipeline->BindGraphicsUniformBuffer(*pipeline, descSet, uniformBuffer, 0, sizeof(ShaderUniform), 0);
      pipeline->BindGraphicsImageView(*pipeline, descSet, r.getTextureSystem()->GetImageView({ 0 }), vk::ImageLayout::eShaderReadOnlyOptimal, r.getTextureSystem()->GetSampler(), 1);

      // Begin & resets the command buffer
      ctx.cmdBuffer.Begin();
      // Use the RenderPass from the pipeline we built
      std::vector<vk::ImageView> renderTarget{ ctx.imageView, ctx.depthImageView };
      ctx.cmdBuffer.WithRenderPass(*pipeline, renderTarget, glm::uvec2(width, height), [&]() {
        // Bind the pipeline to use
        ctx.cmdBuffer.BindPipeline(*pipeline);
        // Bind the vertex buffer
        ctx.cmdBuffer.BindVertexBuffer(vertexBinding, vertexBuffer, 0);
        // Bind the index buffer
        ctx.cmdBuffer.BindIndexBuffer(indexBuffer, 0);
        // Bind the descriptor sets (uniform buffer, texture, etc.)
        ctx.cmdBuffer.BindGraphicsDescSets(*pipeline, descSet);
        // Draw terrain
        ctx.cmdBuffer.PushConstants(*pipeline, vk::ShaderStageFlagBits::eVertex, 0, terrainTransform);
        ctx.cmdBuffer.DrawIndexed(uint32_t(indicies.size()), 0, 0);

        });
      // End the recording of command buffer
      ctx.cmdBuffer.End();

      // After this callback ends, the renderer will submit the recorded commands, and present the image when it's rendered
    },
    // GUI Thread
    [&]() {
      // Render a camera control GUI through ImGui
      ImGui::Begin("Example Window");
      ImGui::DragFloat3("Camera Look At", &cameraLookAt[0], 0.01f);
      ImGui::DragFloat("Camera Orbit Radius", &cameraOrbitRadius, 0.01f);
      ImGui::DragFloat("Camera Orbit Height", &cameraOrbitHeight, 0.01f);
      ImGui::End();
    },
    // Cleanup
    [&]() {

    }
    );
}