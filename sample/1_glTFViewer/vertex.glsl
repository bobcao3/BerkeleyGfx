#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UniformBuffer
{
  mat4 model;
  mat4 viewProj;
} ubo;

void main() {
  vec4 position = vec4(inPosition, 1.0);
  position = ubo.model * position;
  position = ubo.viewProj * position;

  gl_Position = position;
  fragColor = inColor;
  uv = inUV;
}