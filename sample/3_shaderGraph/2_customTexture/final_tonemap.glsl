#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 UV;

layout(binding = 7) uniform sampler2D output0;
layout(binding = 8) uniform sampler2D stoneTexture;

layout(push_constant) uniform Parameters {
  float gamma;
  vec3 colorFilter;
};

void main()
{
  vec3 lastStageColor = (1.0 - texture(output0, UV).r) * texture(stoneTexture, UV).rgb;

  outColor = vec4(pow(lastStageColor, vec3(1.0 / gamma)) * colorFilter, 1.0);
}