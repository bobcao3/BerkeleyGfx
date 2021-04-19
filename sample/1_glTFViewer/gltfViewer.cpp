#include "berkeley_gfx.hpp"
#include "renderer.hpp"
#include "pipelines.hpp"
#include "command_buffer.hpp"
#include "buffer.hpp"
#include "texture_system.hpp"
#include "mesh_system.hpp"

#include <string>
#include <fstream>
#include <streambuf>

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace BG;

// String for storing the shaders
std::string vertexShader;
std::string fragmentShader;

// Each draw cmd draws a child node of the glTF
// Which uses a subsection of the index buffer (firstIndex + indexCount)
// The index values are offsetted with an additional vertex offset.
// (e.g. index N means the vertexOffset + Nth element of the vertex buffer)
struct DrawCmd
{
  uint32_t indexCount = 0;
  uint32_t firstIndex = 0;
  uint32_t vertexOffset = 0;
};

struct ShaderUniform
{
  glm::mat4 viewProjMtx;
};

glm::mat4 viewMtx;
glm::mat4 projMtx;

// Read the shaders into string from file
void load_shader_file()
{
  std::ifstream tf(SRC_DIR"/sample/1_glTFViewer/fragment.glsl");
  fragmentShader = std::string((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());

  std::ifstream tv(SRC_DIR"/sample/1_glTFViewer/vertex.glsl");
  vertexShader = std::string((std::istreambuf_iterator<char>(tv)), std::istreambuf_iterator<char>());
}

// Main function
int main(int, char**)
{
  spdlog::set_level(spdlog::level::debug);

  // Load the shader file into string
  load_shader_file();

  // Instantiate the Berkeley Gfx renderer & the backend
  Renderer r("Sample Project - glTF Viewer");

  Pipeline::InitBackend();

  std::unique_ptr<Pipeline> pipeline;

  // Our GPU buffers holding the vertices and the indices
  std::shared_ptr<Buffer> vertexBuffer, indexBuffer;
  Buffer* uniformBuffer;

  BG::VertexBufferBinding vertexBinding;

  // Camera control parameters
  glm::vec3 cameraLookAt = glm::vec3(0.0);
  float cameraOrbitRadius = 1.0;
  float cameraOrbitHeight = 0.0;

  float globalScale = 1.0;
  bool yUp = true;
  glm::mat4 globalTransform = glm::scale(yUp ? glm::mat4(1.0) : glm::mat4(
    glm::vec4(1.0, 0.0, 0.0, 0.0),
    glm::vec4(0.0, 0.0, 1.0, 0.0),
    glm::vec4(0.0, 1.0, 0.0, 0.0),
    glm::vec4(0.0, 0.0, 0.0, 1.0)
  ), glm::vec3(globalScale));

  std::vector<MeshSystem::Node> nodes;
  MeshSystem::Node* rootNode;

  std::unordered_map<const MeshSystem::Node*, DrawCmd> drawObjects;

  r.Run(
    // Init
    [&]() {
      // Load model
      auto pair = MeshSystem::Loader::FromGltf(r, SRC_DIR"/assets/glTF-Sample-Models/2.0/MaterialsVariantsShoe/glTF/MaterialsVariantsShoe.gltf");
      nodes = std::move(pair.first);
      rootNode = pair.second;

      uint32_t numVertices = 0;
      uint32_t numIndices = 0;

      for (auto& n : nodes)
      {
        numVertices += n.GetVertices().size();
        numIndices += n.GetIndices().size();
      }

      // Allocate buffers on GPU, and flag it as a Vertex Buffer / Index buffer
      vertexBuffer = r.getMemoryAllocator().AllocCPU2GPU(numVertices * sizeof(MeshSystem::Vertex), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      indexBuffer = r.getMemoryAllocator().AllocCPU2GPU(numIndices * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      // Map the GPU buffers into the CPU's memory space
      MeshSystem::Vertex* vertexBufferGPU = vertexBuffer->Map<MeshSystem::Vertex>();
      uint32_t* indexBufferGPU = indexBuffer->Map<uint32_t>();
      // Copy our vertex list into GPU buffer
      uint32_t currentVertex = 0;
      uint32_t currentIndex = 0;
      for (auto& n : nodes)
      {
        if (n.HasMesh())
        {
          DrawCmd obj = { n.GetIndices().size(), currentIndex, currentVertex };

          drawObjects[&n] = obj;

          std::copy(n.GetIndices().cbegin(), n.GetIndices().cend(), &indexBufferGPU[currentIndex]);
          std::copy(n.GetVertices().cbegin(), n.GetVertices().cend(), &vertexBufferGPU[currentVertex]);

          currentIndex += n.GetIndices().size();
          currentVertex += n.GetVertices().size();
        }
      }
      // Unmap the GPU buffers
      vertexBuffer->UnMap();
      indexBuffer->UnMap();

      // Compute a centroid to place our camera
      glm::vec3 min = glm::vec3(INFINITY), max = glm::vec3(-INFINITY);
      rootNode->ForEach(globalTransform, [&](const MeshSystem::Node& n, glm::mat4 transform) {
        if (n.HasMesh())
        {
          glm::vec3 lmin = glm::vec3(INFINITY), lmax = glm::vec3(-INFINITY);
          for (const auto& v : n.GetVertices())
          {
            lmin = glm::min(lmin, v.pos);
            lmax = glm::max(lmax, v.pos);
          }
          min = glm::min(min, glm::vec3(transform * glm::vec4(lmin, 1.0f)));
          max = glm::max(max, glm::vec3(transform * glm::vec4(lmax, 1.0f)));
        }
        });
      cameraLookAt = (max + min) * 0.5f;

      // Create a empty pipline
      pipeline = r.CreatePipeline();
      // Add a vertex binding
      vertexBinding = pipeline->AddVertexBuffer<MeshSystem::Vertex>();
      // Specify two vertex input attributes from the binding
      pipeline->AddAttribute(vertexBinding, 0, vk::Format::eR32G32B32Sfloat, offsetof(MeshSystem::Vertex, pos));
      pipeline->AddAttribute(vertexBinding, 1, vk::Format::eR32G32B32Sfloat, offsetof(MeshSystem::Vertex, normal));
      pipeline->AddAttribute(vertexBinding, 2, vk::Format::eR32G32Sfloat, offsetof(MeshSystem::Vertex, uv0));
      pipeline->AddAttribute(vertexBinding, 3, vk::Format::eR32Sint, offsetof(MeshSystem::Vertex, materialIndex));
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

      glm::mat4 globalTransform = glm::scale(yUp ? glm::mat4(1.0) : glm::mat4(
        glm::vec4(1.0, 0.0, 0.0, 0.0),
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 0.0, 0.0),
        glm::vec4(0.0, 0.0, 0.0, 1.0)
      ), glm::vec3(globalScale));

      // Prepare uniform buffer (view & projection matrix)
      viewMtx = glm::lookAt(glm::vec3(cos(ctx.time) * cameraOrbitRadius, cameraOrbitHeight, sin(ctx.time) * cameraOrbitRadius) + cameraLookAt, cameraLookAt, glm::vec3(0.0, 1.0, 0.0));
      projMtx = glm::perspective(glm::radians(45.0f), float(width) / float(height), 0.01f, 1000.0f);
      projMtx[1][1] *= -1.0;

      // Map & upload the constants
      uniformBuffer = r.getMemoryAllocator().AllocTransient(sizeof(ShaderUniform) * r.getSwapchainImageViews().size(), vk::BufferUsageFlagBits::eUniformBuffer);
      ShaderUniform* uniformBufferGPU = uniformBuffer->Map<ShaderUniform>();
      uniformBufferGPU->viewProjMtx = projMtx * viewMtx;
      uniformBuffer->UnMap();

      // Allocate descriptor sets & bind uniforms
      auto descSet = pipeline->AllocDescSet(ctx.descPool, r.getTextureSystem().GetNumImageViews() + 1);
      pipeline->BindGraphicsUniformBuffer(*pipeline, descSet, *uniformBuffer, 0, sizeof(ShaderUniform), 0);

      for (int i = 0; i < r.getTextureSystem().GetNumImageViews(); i++)
      {
        pipeline->BindGraphicsImageView(*pipeline, descSet, r.getTextureSystem().GetImageView({ i }), vk::ImageLayout::eShaderReadOnlyOptimal, r.getTextureSystem().GetSampler(), 15, i);
      }

      // Begin & resets the command buffer
      ctx.cmdBuffer.Begin();
      // Use the RenderPass from the pipeline we built
      std::vector<vk::ImageView> renderTarget{ ctx.imageView, ctx.depthImageView };
      ctx.cmdBuffer.WithRenderPass(*pipeline, renderTarget, glm::uvec2(width, height), [&](){
        // Bind the pipeline to use
        ctx.cmdBuffer.BindPipeline(*pipeline);
        // Bind the vertex buffer
        ctx.cmdBuffer.BindVertexBuffer(vertexBinding, *vertexBuffer, 0);
        // Bind the index buffer
        ctx.cmdBuffer.BindIndexBuffer(*indexBuffer, 0);
        // Bind the descriptor sets (uniform buffer, texture, etc.)
        ctx.cmdBuffer.BindGraphicsDescSets(*pipeline, descSet);
        // Draw objects
        rootNode->ForEach(globalTransform, [&](const MeshSystem::Node& n, glm::mat4 transform) {
          if (n.HasMesh())
          {
            auto& drawCmd = drawObjects[&n];
            ctx.cmdBuffer.PushConstants(*pipeline, vk::ShaderStageFlagBits::eVertex, 0, transform);
            ctx.cmdBuffer.DrawIndexed(drawCmd.indexCount, drawCmd.firstIndex, drawCmd.vertexOffset);
          }
          });
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
      ImGui::DragFloat("Global Scale", &globalScale, 0.01f);
      ImGui::Checkbox("Is Y axis up", &yUp);

      rootNode->ForEach(glm::mat4(1.0), [&](const MeshSystem::Node& n, glm::mat4 transform) {
        if (ImGui::TreeNodeEx(&n, 0, "Node 0x%x", &n))
        {
          for (int i = 0; i < 4; i++) ImGui::Text("%f %f %f %f", transform[i].x, transform[i].y, transform[i].z, transform[i].w);
          ImGui::TreePop();
        }
        });

      ImGui::End();
    },
    // Cleanup
    [&]() {
    }
  );

  return 0;
}
