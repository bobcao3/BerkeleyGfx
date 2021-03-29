#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 fragColor;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform UniformBuffer
{
  mat4 model;
  mat4 viewProj;
} ubo;

void main() {
  vec4 position = vec4(inPosition, 1.0);
  position = ubo.model * position;
  position = ubo.viewProj * position;
  position.xyz /= position.w;
  position.w = 1.0;

  gl_Position = position;
  fragColor = inColor;
}