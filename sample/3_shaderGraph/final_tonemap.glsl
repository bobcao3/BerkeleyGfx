#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 UV;

layout(binding = 7) uniform sampler2D output0;

void main()
{
  vec3 lastStageColor = texelFetch(output0, ivec2(gl_FragCoord.st), 0).rgb;

  outColor = vec4(pow(lastStageColor, vec3(2.2)), 1.0);
}