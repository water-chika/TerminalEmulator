#version 460

layout(location=0) in vec2 coord;
layout(location=0) out vec4 out_color;
layout(binding=0) uniform sampler2D tex_sampler;

void main() {
    float a = texture(tex_sampler, coord).x;
    vec3 color = vec3(0, 0, 0);
    out_color = vec4(color,a);
}