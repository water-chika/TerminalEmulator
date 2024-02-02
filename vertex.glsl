#version 460

struct vertex{
    vec4 pos;
};

layout(location=0) in vec4 pos;

void main() {
    gl_Position = pos;
}