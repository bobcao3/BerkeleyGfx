#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 UV;

layout(binding = 7) uniform sampler2D output0;

/*
layout(binding = 1) uniform Parameters {
  float gamma;
  vec3 colorFilter;
};
*/

float gamma = 2.2;
vec3 colorFilter = vec3(1.0);

void main()
{
  vec3 lastStageColor = texelFetch(output0, ivec2(gl_FragCoord.st), 0).rgb;

  outColor = vec4(pow(lastStageColor, vec3(gamma)) * colorFilter, 1.0);
}