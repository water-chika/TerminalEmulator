#version 460

#extension GL_EXT_mesh_shader : enable

void main() {
    EmitMeshTasksEXT(16, 1, 1);
}