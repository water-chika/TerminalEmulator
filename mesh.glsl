#version 460
#extension GL_EXT_mesh_shader : enable

layout(local_size_x=8,local_size_y=1) in;
layout(max_primitives=8*2, max_vertices=8*6) out;
layout(triangles) out;


void main(){
    vec2 pos = gl_LocalInvocationID.xy * 0.1 + vec2(-1.0, -1.0);
    vec2 pos1 = pos + vec2(1,0)*0.1;
    vec2 pos2 = pos + vec2(0,1)*0.1;
    vec2 pos3 = pos + vec2(1,1)*0.1;
    uint vertex_per_thread = 6;
    uint triangle_per_thread = 2;
    uint vertex_start_index = vertex_per_thread*gl_LocalInvocationIndex;
    uint index_start_index = triangle_per_thread* gl_LocalInvocationIndex;
    gl_MeshVerticesEXT[vertex_start_index + 0].gl_Position = vec4(pos,0,1);
    gl_MeshVerticesEXT[vertex_start_index + 1].gl_Position = vec4(pos1,0,1);
    gl_MeshVerticesEXT[vertex_start_index + 2].gl_Position = vec4(pos2,0,1);
    gl_MeshVerticesEXT[vertex_start_index + 3].gl_Position = vec4(pos1,0,1);
    gl_MeshVerticesEXT[vertex_start_index + 4].gl_Position = vec4(pos2,0,1);
    gl_MeshVerticesEXT[vertex_start_index + 5].gl_Position = vec4(pos3,0,1);
    gl_PrimitiveTriangleIndicesEXT[index_start_index] = uvec3(vertex_start_index,vertex_start_index+1,vertex_start_index+2);
    gl_PrimitiveTriangleIndicesEXT[index_start_index+1] = uvec3(vertex_start_index+3,vertex_start_index+4,vertex_start_index+5);
    uint work_group_size = gl_WorkGroupSize.x;
    SetMeshOutputsEXT(vertex_per_thread*work_group_size, triangle_per_thread*work_group_size);
}
