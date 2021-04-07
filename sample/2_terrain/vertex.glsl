#version 450

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 worldPosition;

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform UniformBuffer
{
  mat4 viewProjMtx;
};

layout(binding = 1) uniform sampler2D tex;

layout(push_constant) uniform PushData {
  mat4 modelMtx;
};

void main() {
  vec4 position = vec4(inPosition, 1.0);
  
  float height = texture(tex, inPosition.xz).r;
  position.y = height;
  
  position = modelMtx * position;
  worldPosition = position.xyz;
  position = viewProjMtx * position;

  // 0.0 ~ 0.3 brown, transition radius = 0.15
  // 0.3 ~ 0.7 green
  // 0.7 ~ 1.0 white, transition radius = 0.05
  vec3 green = vec3(0.1, 0.6, 0.2);
  vec3 brown = vec3(0.4, 0.15, 0.05);
  vec3 white = vec3(1.0);

  vec3 color = mix(brown * (height + 0.1), green, smoothstep(0.15, 0.45, height));
  color = mix(color, white, smoothstep(0.65, 0.75, height));

  gl_Position = position;
  fragColor = color;
}