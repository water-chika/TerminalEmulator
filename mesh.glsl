#version 460
#extension GL_EXT_mesh_shader : enable

layout(local_size_x=8,local_size_y=1) in;
layout(max_primitives=8*2, max_vertices=8*6) out;
layout(triangles) out;

layout(location=0) out vec2 coord[];

uint vertex_per_thread = 6;
uint triangle_per_thread = 2;
uint vertex_start_index = vertex_per_thread*gl_LocalInvocationIndex;
uint index_start_index = triangle_per_thread* gl_LocalInvocationIndex;

void set_pos(uint index, vec2 pos) {
    gl_MeshVerticesEXT[vertex_start_index + index].gl_Position = vec4(pos, 0, 1);
}
void set_coord(uint index, vec2 c) {
    coord[vertex_start_index + index] = c;
}

void main(){
    vec2 pos = gl_LocalInvocationID.xy * 0.1 + vec2(-1.0, -1.0);
    vec2 pos1 = pos + vec2(1,0)*0.1;
    vec2 pos2 = pos + vec2(0,1)*0.1;
    vec2 pos3 = pos + vec2(1,1)*0.1;

    set_pos(0, pos); set_coord(0, vec2(0,0));
    set_pos(1, pos1); set_coord(1, vec2(1,0));
    set_pos(2, pos2); set_coord(2, vec2(0,1));
    set_pos(3, pos1); set_coord(3, vec2(1,0));
    set_pos(4, pos2); set_coord(4, vec2(0,1));
    set_pos(5, pos3); set_coord(5, vec2(1,1));
    gl_PrimitiveTriangleIndicesEXT[index_start_index] = uvec3(vertex_start_index,vertex_start_index+1,vertex_start_index+2);
    gl_PrimitiveTriangleIndicesEXT[index_start_index+1] = uvec3(vertex_start_index+3,vertex_start_index+4,vertex_start_index+5);
    uint work_group_size = gl_WorkGroupSize.x;
    SetMeshOutputsEXT(vertex_per_thread*work_group_size, triangle_per_thread*work_group_size);
}
