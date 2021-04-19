#version 450

layout(location = 0) out vec2 uv;
layout(location = 1) flat out int materialId;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in int inMaterialId;

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
  uv = inUV;
  materialId = inMaterialId;
}