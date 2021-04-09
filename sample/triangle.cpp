#include "berkeley_gfx.hpp"
#include "renderer.hpp"
#include "pipelines.hpp"
#include "command_buffer.hpp"
#include "buffer.hpp"

using namespace BG;

std::string vertexShader = R"V0G0N(

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 fragColor;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}

)V0G0N";

std::string fragmentShader = R"V0G0N(

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}

)V0G0N";

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
};

const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}}
};

int main(int, char**)
{
  Renderer r("Sample Project - Hello Triangle", true);

  Pipeline::InitBackend();

  std::shared_ptr<Pipeline> pipeline;

  std::vector<vk::UniqueFramebuffer> framebuffers;

  std::shared_ptr<Buffer> vertexBuffer;

  BG::VertexBufferBinding vertexBinding;

  r.Run(
    // Init
    [&]() {
      // Allocate a buffer on GPU, and flag it as a Vertex Buffer & can be copied towards
      vertexBuffer = r.getMemoryAllocator()->AllocCPU2GPU(vertices.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      // Map the GPU buffer into the CPU's memory space
      Vertex* vertexBufferGPU = vertexBuffer->Map<Vertex>();
      // Copy our vertex list into GPU buffer
      std::copy(vertices.begin(), vertices.end(), vertexBufferGPU);
      // Unmap the GPU buffer
      vertexBuffer->UnMap();

      // Create a empty pipline
      pipeline = r.CreatePipeline();
      // Add a vertex binding
      vertexBinding = pipeline->AddVertexBuffer<Vertex>();
      // Specify two vertex input attributes from the binding
      pipeline->AddAttribute(vertexBinding, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos));
      pipeline->AddAttribute(vertexBinding, 1, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color));
      // Add shaders
      pipeline->AddFragmentShaders(fragmentShader);
      pipeline->AddVertexShaders(vertexShader);
      // Set the viewport
      pipeline->SetViewport(float(r.getWidth()), float(r.getHeight()));
      // Add an attachment for the pipeline to render to
      pipeline->AddAttachment(r.getSwapChainFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
      // Build the pipeline
      pipeline->BuildPipeline();

      int width = r.getWidth(), height = r.getHeight();

      // Create a Framebuffer (specifications of the render targets) for each SwapChain image
      for (auto& imageView : r.getSwapchainImageViews())
      {
        std::vector<vk::ImageView> renderTarget{ imageView.get() };
        framebuffers.push_back(r.CreateFramebuffer(pipeline->GetRenderPass(), renderTarget, width, height));
      }
    },
    // Render
    [&](Renderer::Context& ctx) {
      int width = r.getWidth(), height = r.getHeight();

      // Begin & resets the command buffer
      ctx.cmdBuffer.Begin();
      // Use the RenderPass from the pipeline we built
      ctx.cmdBuffer.WithRenderPass(*pipeline, framebuffers[ctx.imageIndex].get(), glm::uvec2(width, height), [&](){
        // Bind the pipeline to use
        ctx.cmdBuffer.BindPipeline(*pipeline);
        // Bind the vertex buffer
        ctx.cmdBuffer.BindVertexBuffer(vertexBinding, vertexBuffer, 0);
        // Draw 3 vertices from the vertex buffer
        ctx.cmdBuffer.Draw(3);
        });
      // End the recording of command buffer
      ctx.cmdBuffer.End();

      // After this callback ends, the renderer will submit the recorded commands, and present the image when it's rendered
    },
    // Render GUI
    []() {
    },
    // Cleanup
    [&]() {

    }
  );

  return 0;
}
