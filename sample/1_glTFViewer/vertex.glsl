#version 450

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UniformBuffer
{
  mat4 viewProjMtx;
};

layout(push_constant) uniform PushData {
  mat4 modelMtx;
};

void main() {
  vec4 position = vec4(inPosition, 1.0);
  position = modelMtx * position;
  position = viewProjMtx * position;

  gl_Position = position;
  fragColor = inColor;
  uv = inUV;
}