#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 uv;
layout(location = 1) flat in int materialId;

layout(location = 0) out vec4 outColor;

layout(binding = 15) uniform sampler2D tex[];

void main() {
    outColor = vec4(texture(tex[nonuniformEXT(materialId)], uv).rgb, 1.0);
}