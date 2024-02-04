#version 460

layout(location=0) in vec2 coord;
layout(location=0) out vec4 out_color;
layout(binding=0) uniform sampler2D tex_sampler;

void main() {
    out_color = vec4(coord, 0, 1);
}