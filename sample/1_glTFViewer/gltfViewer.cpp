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

// Import the tinyGlTF library to load glTF models
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

using namespace BG;

// String for storing the shaders
std::string vertexShader;
std::string fragmentShader;

// Our vertex format
struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 uv;
};

// Our CPU side index / vertex buffer
std::vector<Vertex> vertices;
std::vector<uint32_t> indicies;

// Each draw cmd draws a child node of the glTF
// Which uses a subsection of the index buffer (firstIndex + indexCount)
// The index values are offsetted with an additional vertex offset.
// (e.g. index N means the vertexOffset + Nth element of the vertex buffer)
struct DrawCmd
{
  uint32_t indexCount;
  uint32_t firstIndex;
  uint32_t vertexOffset;
  glm::mat4 transform;
  int materialId;
};

struct ShaderUniform
{
  glm::mat4 viewProjMtx;
};

glm::mat4 viewMtx;
glm::mat4 projMtx;

// List of draw commands
std::vector<DrawCmd> drawObjects;

// Read the shaders into string from file
void load_shader_file()
{
  std::ifstream tf(SRC_DIR"/sample/1_glTFViewer/fragment.glsl");
  fragmentShader = std::string((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());

  std::ifstream tv(SRC_DIR"/sample/1_glTFViewer/vertex.glsl");
  vertexShader = std::string((std::istreambuf_iterator<char>(tv)), std::istreambuf_iterator<char>());
}

// Documents for the glTF format:
// https://raw.githubusercontent.com/KhronosGroup/glTF/master/specification/2.0/figures/gltfOverview-2.0.0b.png
// Here for sake of simplicity we just flatten the entire scene hierarchy to a vertex buffer, a index buffer, and a series of offsets
void load_gltf_node(tinygltf::Model& model, int nodeId, glm::mat4 transform)
{
  auto& node = model.nodes[nodeId];

  // Hierarchical transform
  glm::mat4 localTransform;
  
  localTransform = node.matrix.size() == 16 ? glm::mat4(
    node.matrix[     0], node.matrix[     1], node.matrix[     2], node.matrix[     3], 
    node.matrix[ 4 + 0], node.matrix[ 4 + 1], node.matrix[ 4 + 2], node.matrix[ 4 + 3],
    node.matrix[ 8 + 0], node.matrix[ 8 + 1], node.matrix[ 8 + 2], node.matrix[ 8 + 3],
    node.matrix[12 + 0], node.matrix[12 + 1], node.matrix[12 + 2], node.matrix[12 + 3]
  ) : glm::mat4(1.0);

  localTransform = localTransform * transform;

  // The node contains a mesh
  if (node.mesh >= 0)
  {
    auto& mesh = model.meshes[node.mesh];

    spdlog::info("======== NODE {} ========", nodeId);

    // Iterate through all primitives of the mesh
    for (auto& primitive : mesh.primitives)
    {
      // Get the vertex position accessor (and relavent buffers)
      auto& positionAccessor = model.accessors[primitive.attributes["POSITION"]];
      spdlog::info("{}x{}, offset = {}", positionAccessor.count, positionAccessor.ByteStride(model.bufferViews[positionAccessor.bufferView]), positionAccessor.byteOffset);
      
      auto& positionBufferView = model.bufferViews[positionAccessor.bufferView];
      auto& positionBuffer = model.buffers[positionBufferView.buffer];

      size_t positionBufferStride = positionAccessor.ByteStride(model.bufferViews[positionAccessor.bufferView]);

      // Get the index accessor (and relavent buffers)
      auto& indexAccessor = model.accessors[primitive.indices];
      spdlog::info("{}x{}, offset = {}", indexAccessor.count, indexAccessor.ByteStride(model.bufferViews[indexAccessor.bufferView]), indexAccessor.byteOffset);

      auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
      auto& indexBuffer = model.buffers[indexBufferView.buffer];

      size_t indexBufferStride = indexAccessor.ByteStride(model.bufferViews[indexAccessor.bufferView]);

      // Get the texture UV accessor (and relavent buffers)
      auto& uvAccessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
      spdlog::info("{}x{}, offset = {}", uvAccessor.count, uvAccessor.ByteStride(model.bufferViews[uvAccessor.bufferView]), uvAccessor.byteOffset);

      auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
      auto& uvBuffer = model.buffers[uvBufferView.buffer];

      size_t uvBufferStride = uvAccessor.ByteStride(model.bufferViews[uvAccessor.bufferView]);

      // Record the current size of our vertex buffer and index buffer
      // We will be pushing new data into these two vectors
      // The current size will be the starting offset of this draw command
      size_t startVertex = vertices.size();
      size_t startIndex = indicies.size();

      // Push all vertices
      for (size_t index = 0; index < positionAccessor.count; index++)
      {
        // Get the base address and store the vertex
        float* elementBase = (float*)((uint8_t*)(positionBuffer.data.data()) + positionBufferView.byteOffset + positionAccessor.byteOffset + positionBufferStride * index);
        float* uvElementBase = (float*)((uint8_t*)(uvBuffer.data.data()) + uvBufferView.byteOffset + uvAccessor.byteOffset + uvBufferStride * index);
        Vertex v;
        v.pos = glm::vec3(elementBase[0], elementBase[1], elementBase[2]);
        v.color = glm::vec3(0.7);
        v.uv = glm::vec2(uvElementBase[0], uvElementBase[1]);
        vertices.push_back(v);
      }

      // Push all indices
      for (size_t index = 0; index < indexAccessor.count; index++)
      {
        uint16_t* elementBase = (uint16_t*)((uint8_t*)(indexBuffer.data.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset + indexBufferStride * index);
        indicies.push_back(uint32_t(elementBase[0]));
      }

      // Append a new draw command to the list
      drawObjects.push_back({
        uint32_t(indexAccessor.count),
        uint32_t(startIndex),
        uint32_t(startVertex),
        localTransform
        });
    }
  }

  // Recursively go through all child nodes
  for (int childNodeId : node.children)
  {
    load_gltf_node(model, childNodeId, localTransform);
  }
}

// The function that loads the glTF file and initiate the recursive call to flatten the hierarchy
void load_gltf_model(Renderer& r)
{
  std::string model_file = SRC_DIR"/assets/glTF-Sample-Models/2.0/WaterBottle/glTF/WaterBottle.gltf";

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  // Here we are loadinga *.gltf file which is an ASCII json.
  // If we want to load *.glb (binary package), we should use the LoadBinaryFromFile function
  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, model_file);

  // Check whether the library successfully loaded the glTF model
  if (!warn.empty()) {
    spdlog::warn("Warn: %s\n", warn.c_str());
  }

  if (!err.empty()) {
    spdlog::error("Err: %s\n", err.c_str());
    throw std::runtime_error("glTF parsing error");
  }

  if (!ret) {
    spdlog::error("Failed to parse glTF");
    throw std::runtime_error("Fail to parse glTF");
  }

  // Get the default scene
  auto& scene = model.scenes[model.defaultScene];

  glm::mat4 transform = glm::mat4(1.0);

  // Initiate recursive call on the root nodes
  for (int sceneRootNodeId : scene.nodes)
  {
    load_gltf_node(model, sceneRootNodeId, transform);
  }

  for (auto& m : model.materials)
  {
    
  }

  for (auto& img : model.images)
  {
    spdlog::info("Size {}, width {}, height {}, bit depth {}, {}", img.image.size(), img.width, img.height, img.bits, img.image.size() / img.width / img.height);

    r.getTextureSystem()->AddTexture(img.image.data(), img.width, img.height, img.image.size(), vk::Format::eR8G8B8A8Srgb);
  }

  spdlog::info("======== glTF load finished ========");
}

// Main function
int main(int, char**)
{
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
  glm::vec3 cameraLookAt = glm::vec3(0.0);
  float cameraOrbitRadius = 1.0;
  float cameraOrbitHeight = 0.0;

  r.Run(
    // Init
    [&]() {
      // Load the glTF models into vertex and index buffer
      load_gltf_model(r);

      // Allocate a buffer on GPU, and flag it as a Vertex Buffer & can be copied towards
      vertexBuffer = r.getMemoryAllocator()->AllocCPU2GPU(vertices.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      // Map the GPU buffer into the CPU's memory space
      Vertex* vertexBufferGPU = vertexBuffer->Map<Vertex>();
      // Copy our vertex list into GPU buffer
      std::copy(vertices.begin(), vertices.end(), vertexBufferGPU);
      // Unmap the GPU buffer
      vertexBuffer->UnMap();

      // Allocate a buffer on GPU, and flag it as a Index Buffer & can be copied towards
      indexBuffer = r.getMemoryAllocator()->AllocCPU2GPU(indicies.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
      // Map the GPU buffer into the CPU's memory space
      uint32_t* indexBufferGPU = indexBuffer->Map<uint32_t>();
      // Copy our vertex list into GPU buffer
      std::copy(indicies.begin(), indicies.end(), indexBufferGPU);
      // Unmap the GPU buffer
      indexBuffer->UnMap();

      uniformBuffer = r.getMemoryAllocator()->AllocCPU2GPU(sizeof(ShaderUniform) * r.getSwapchainImageViews().size(), vk::BufferUsageFlagBits::eUniformBuffer);

      // Create a empty pipline
      pipeline = r.CreatePipeline();
      // Add a vertex binding
      vertexBinding = pipeline->AddVertexBuffer<Vertex>();
      // Specify two vertex input attributes from the binding
      pipeline->AddAttribute(vertexBinding, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
      pipeline->AddAttribute(vertexBinding, 1, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color));
      pipeline->AddAttribute(vertexBinding, 2, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));
      // Specify push constants
      pipeline->AddPushConstant(0, sizeof(glm::mat4), vk::ShaderStageFlagBits::eVertex);
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
      viewMtx = glm::lookAt(glm::vec3(cos(ctx.time) * cameraOrbitRadius, cameraOrbitHeight, sin(ctx.time) * cameraOrbitRadius), cameraLookAt, glm::vec3(0.0, 1.0, 0.0));
      projMtx = glm::perspective(glm::radians(45.0f), float(width) / float(height), 0.1f, 10.0f);
      projMtx[1][1] *= -1.0;

      // Map & upload the constants
      ShaderUniform* uniformBufferGPU = uniformBuffer->Map<ShaderUniform>();
      auto& uniform = uniformBufferGPU[ctx.imageIndex];
      uniform.viewProjMtx = projMtx * viewMtx;
      uniformBuffer->UnMap();

      // Allocate descriptor sets & bind uniforms
      auto descSet = pipeline->AllocDescSet(ctx.descPool);
      pipeline->BindGraphicsUniformBuffer(*pipeline, descSet, uniformBuffer, sizeof(ShaderUniform) * ctx.imageIndex, sizeof(ShaderUniform), 0);
      pipeline->BindGraphicsImageView(*pipeline, descSet, r.getTextureSystem()->GetImageView({ 0 }), vk::ImageLayout::eShaderReadOnlyOptimal, r.getTextureSystem()->GetSampler(), 1);

      // Begin & resets the command buffer
      ctx.cmdBuffer.Begin();
      // Use the RenderPass from the pipeline we built
      std::vector<vk::ImageView> renderTarget{ ctx.imageView, ctx.depthImageView };
      ctx.cmdBuffer.WithRenderPass(*pipeline, renderTarget, glm::uvec2(width, height), [&](){
        // Bind the pipeline to use
        ctx.cmdBuffer.BindPipeline(*pipeline);
        // Bind the vertex buffer
        ctx.cmdBuffer.BindVertexBuffer(vertexBinding, vertexBuffer, 0);
        // Bind the index buffer
        ctx.cmdBuffer.BindIndexBuffer(indexBuffer, 0);
        // Bind the descriptor sets (uniform buffer, texture, etc.)
        ctx.cmdBuffer.BindGraphicsDescSets(*pipeline, descSet);
        // Draw objects
        int i = 0;
        for (auto& drawCmd : drawObjects)
        {
          // Set the model matrix using "Push constant"
          ctx.cmdBuffer.PushConstants(*pipeline, vk::ShaderStageFlagBits::eVertex, 0, drawCmd.transform);
          ctx.cmdBuffer.DrawIndexed(drawCmd.indexCount, drawCmd.firstIndex, drawCmd.vertexOffset);
          i++;
        }
        });
      // End the recording of command buffer
      ctx.cmdBuffer.End();

      // After this callback ends, the renderer will submit the recorded commands, and present the image when it's rendered
    },
    // GUI Thread
    [&]() {
      // Render a camera control GUI through ImGui
      ImGui::Begin("Example Window");
      ImGui::DragFloat3("Camera Look At", &cameraLookAt[0], 0.01);
      ImGui::DragFloat("Camera Orbit Radius", &cameraOrbitRadius, 0.01);
      ImGui::DragFloat("Camera Orbit Height", &cameraOrbitHeight, 0.01);
      ImGui::End();
    },
    // Cleanup
    [&]() {

    }
  );

  return 0;
}
