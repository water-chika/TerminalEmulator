#version 460

layout(location=0) in vec4 vertex;

layout(location=0) out vec2 coord;

void main() {
    vec2 pos = vertex.xy;
    gl_Position = vec4(pos, 0, 1);
    coord = vertex.zw;
}