#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 UV;

layout(binding = 7) uniform sampler2D output0;

layout(push_constant) uniform Parameters {
  float gamma;
  vec3 colorFilter;
};

void main()
{
  vec3 lastStageColor = texelFetch(output0, ivec2(gl_FragCoord.st), 0).rgb;

  outColor = vec4(pow(lastStageColor, vec3(1.0 / gamma)) * colorFilter, 1.0);
}