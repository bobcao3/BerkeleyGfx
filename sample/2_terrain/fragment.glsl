#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 worldPosition;

layout(location = 0) out vec4 outColor;

void main() {
  vec3 normal = normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));

  outColor = vec4(fragColor, 1.0) * (normal.z * 0.5 + 0.5);
}